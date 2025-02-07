#include "input.hpp"

#include <algorithm>
#include <cassert>

#include "server.hpp"

void seat::new_input(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_input_device *device = static_cast<wlr_input_device *>(data);

    switch(device->type) {
        case WLR_INPUT_DEVICE_POINTER:
        case WLR_INPUT_DEVICE_TOUCH:
        case WLR_INPUT_DEVICE_TABLET:
            cursor::new_pointer(device);
            break;
        case WLR_INPUT_DEVICE_KEYBOARD:
            keyboard::new_keyboard(device);
            break;
        case WLR_INPUT_DEVICE_SWITCH:
        case WLR_INPUT_DEVICE_TABLET_PAD:
            wlr_log(WLR_ERROR, "TODO");
            exit(EXIT_FAILURE);
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if(!server.keyboards.empty())
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;

    wlr_seat_set_capabilities(server.seat, caps);
}

void seat::request_cursor(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_seat_pointer_request_set_cursor_event *event =
        static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);

    // Check that it's actually being sent by the focused client
    wlr_seat_client *focused_client = server.seat->pointer_state.focused_client;
    if(focused_client == event->seat_client)
        wlr_cursor_set_surface(server.cursor, event->surface, event->hotspot_x, event->hotspot_y);
}

void seat::request_set_selection(wl_listener *listener, void *data) {
    wlr_seat_request_set_selection_event *event =
        static_cast<wlr_seat_request_set_selection_event *>(data);

    wlr_seat_set_selection(Server::instance().seat, event->source, event->serial);
}

void cursor::new_pointer(wlr_input_device *device) {
    wlr_cursor_attach_input_device(Server::instance().cursor, device);
}

void cursor::reset_cursor_mode() {
    Server &server = Server::instance();
    server.cursor_mode = CursorMode::PASSTHROUGH;
    server.grabbed_toplevel = nullptr;
}

void cursor::cursor_motion(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_pointer_motion_event *event = static_cast<wlr_pointer_motion_event *>(data);

    wlr_cursor_move(server.cursor, &event->pointer->base, event->delta_x, event->delta_y);
    server.process_cursor_motion(event->time_msec);
}

void cursor::cursor_motion_absolute(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_pointer_motion_absolute_event *event =
        static_cast<wlr_pointer_motion_absolute_event *>(data);

    wlr_cursor_warp_absolute(server.cursor, &event->pointer->base, event->x, event->y);
    server.process_cursor_motion(event->time_msec);
}

void cursor::cursor_button(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_pointer_button_event *event = static_cast<wlr_pointer_button_event *>(data);
    wlr_seat_pointer_notify_button(server.seat, event->time_msec, event->button, event->state);

    if(event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        reset_cursor_mode();
    }
    else {
        double sx, sy;
        wlr_surface *surface = nullptr;
        xdg_shell::Toplevel *toplevel =
            xdg_shell::desktop_toplevel_at(server.cursor->x, server.cursor->y, surface, sx, sy);
        focus_toplevel(toplevel);
    }
}

void cursor::cursor_axis(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_pointer_axis_event *event = static_cast<wlr_pointer_axis_event *>(data);

    wlr_seat_pointer_notify_axis(server.seat, event->time_msec, event->orientation, event->delta,
                                 event->delta_discrete, event->source, event->relative_direction);
}

void cursor::cursor_frame(wl_listener *listener, void *data) {
    wlr_seat_pointer_notify_frame(Server::instance().seat);
}

keyboard::Keyboard::Keyboard(wlr_keyboard *kb)
    : keyboard(kb),
      modifiers(this, keyboard::modifiers, &kb->events.modifiers),
      key(this, keyboard::key, &kb->events.key),
      destroy(this, keyboard::destroy, &kb->base.events.destroy) {
    Server &server = Server::instance();
    xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);

    // Assign XKB keymap
    wlr_keyboard_set_keymap(keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(keyboard, 25, 500);

    wlr_seat_set_keyboard(server.seat, keyboard);
}

void keyboard::new_keyboard(wlr_input_device *device) {
    Server::instance().keyboards.push_back(
        new keyboard::Keyboard(wlr_keyboard_from_input_device(device)));
}

bool keyboard::handle_keybinding(xkb_keysym_t sym) {
    Server &server = Server::instance();
    switch(sym) {
        case XKB_KEY_Escape:
            wl_display_terminate(server.display);
            break;
        default:
            return false;
    }
    return true;
}

void keyboard::modifiers(wl_listener *listener, void *data) {
    Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
    Server &server = Server::instance();

    wlr_seat_set_keyboard(server.seat, keyboard->keyboard);
    wlr_seat_keyboard_notify_modifiers(server.seat, &keyboard->keyboard->modifiers);
}

void keyboard::key(wl_listener *listener, void *data) {
    Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
    Server &server = Server::instance();
    wlr_keyboard_key_event *event = static_cast<wlr_keyboard_key_event *>(data);

    // libinput keycode -> xkbcommon
    uint32_t keycode = event->keycode + 8;
    // Get a list of keysyms based on the keymap for this keyboard
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->keyboard->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);
    if((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for(int i = 0; i < nsyms; i++) {
            handled = handle_keybinding(syms[i]);
        }
    }

    if(!handled) {
        wlr_seat_set_keyboard(server.seat, keyboard->keyboard);
        wlr_seat_keyboard_notify_key(server.seat, event->time_msec, event->keycode, event->state);
    }
}

void keyboard::destroy(wl_listener *listener, void *data) {
    Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
    Server &server = Server::instance();

    server.keyboards.erase(std::remove(server.keyboards.begin(), server.keyboards.end(), keyboard),
                           server.keyboards.end());

    delete keyboard;
}
