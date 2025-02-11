#include "input.hpp"

#include <algorithm>
#include <cassert>

#include "layer-shell.hpp"
#include "server.hpp"

seat::Seat::Seat(const char *seat_name, wl_display *display, wlr_scene_tree *seat_tree)
    : seat(wlr_seat_create(display, seat_name)),

      scene_tree(wlr_scene_tree_create(seat_tree)),
      drag_icons(wlr_scene_tree_create(scene_tree)),

      request_cursor(this, seat::request_cursor, &seat->events.request_set_cursor),
      request_set_selection(this, seat::request_set_selection,
                            &seat->events.request_set_selection) {
    seat->data = this;
}

void seat::Seat::free_listeners() {
    request_cursor.free();
    request_set_selection.free();
}

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

    wlr_seat_set_capabilities(server.seat.seat, caps);
}

void seat::request_cursor(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_seat_pointer_request_set_cursor_event *event =
        static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);

    // Check that it's actually being sent by the focused client
    wlr_seat_client *focused_client = server.seat.seat->pointer_state.focused_client;
    if(focused_client == event->seat_client)
        wlr_cursor_set_surface(server.cursor.cursor, event->surface, event->hotspot_x,
                               event->hotspot_y);
}

void seat::request_set_selection(wl_listener *listener, void *data) {
    Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;
    wlr_seat_request_set_selection_event *event =
        static_cast<wlr_seat_request_set_selection_event *>(data);

    wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

cursor::Cursor::Cursor()
    :  // cursor is a wlroots utility for tracking the cursor image
      cursor(wlr_cursor_create()),
      // Creates a xcursor manager, that loads Xcursor cursors
      // and manages scaling them
      cursor_mgr(wlr_xcursor_manager_create(nullptr, 24)),
      cursor_mode(CursorMode::PASSTHROUGH),

      motion(this, cursor::cursor_motion, &cursor->events.motion),
      motion_absolute(this, cursor::cursor_motion_absolute, &cursor->events.motion_absolute),
      button(this, cursor::cursor_button, &cursor->events.button),
      axis(this, cursor::cursor_axis, &cursor->events.axis),
      frame(this, cursor::cursor_frame, &cursor->events.frame) {}

cursor::Cursor::~Cursor() {
    wlr_xcursor_manager_destroy(cursor_mgr);
    wlr_cursor_destroy(cursor);
}

void cursor::Cursor::reset_cursor_mode() {
    cursor_mode = CursorMode::PASSTHROUGH;
    grabbed_toplevel = nullptr;
}

void cursor::Cursor::process_motion(uint32_t time) {
    if(cursor_mode == cursor::CursorMode::MOVE) {
        process_cursor_move();
        return;
    }
    else if(cursor_mode == cursor::CursorMode::RESIZE) {
        process_cursor_resize();
        return;
    }

    Server &server = Server::instance();
    double sx, sy;
    wlr_surface *surface = nullptr;

    xdg_shell::Toplevel *toplevel = server.toplevel_at(cursor->x, cursor->y, surface, sx, sy);
    if(toplevel && surface && surface->mapped) {
        wlr_seat_pointer_notify_enter(server.seat.seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server.seat.seat, time, sx, sy);
        return;
    }

    layer_shell::LayerSurface *layer_surface =
        server.layer_surface_at(cursor->x, cursor->y, surface, sx, sy);
    if(layer_surface && surface && surface->mapped) {
        wlr_seat_pointer_notify_enter(server.seat.seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server.seat.seat, time, sx, sy);
        return;
    }

    wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    wlr_seat_pointer_clear_focus(server.seat.seat);
}

void cursor::Cursor::begin_interactive(xdg_shell::Toplevel *toplevel, cursor::CursorMode mode,
                                       uint32_t edges) {
    assert(mode != cursor::CursorMode::PASSTHROUGH);

    grabbed_toplevel = toplevel;
    cursor_mode = mode;

    if(mode == cursor::CursorMode::MOVE) {
        grab_x = cursor->x - toplevel->scene_tree->node.x;
        grab_y = cursor->y - toplevel->scene_tree->node.y;
    }
    else {
        wlr_box *geo_box = &toplevel->toplevel->base->geometry;
        double border_x = (toplevel->scene_tree->node.x + geo_box->x) +
                          ((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
        double border_y = (toplevel->scene_tree->node.y + geo_box->y) +
                          ((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
        grab_x = cursor->x - border_x;
        grab_y = cursor->y - border_y;

        grab_geobox = *geo_box;
        grab_geobox.x += toplevel->scene_tree->node.x;
        grab_geobox.y += toplevel->scene_tree->node.y;

        resize_edges = edges;
    }
}

void cursor::Cursor::process_cursor_move() {
    xdg_shell::Toplevel *toplevel = grabbed_toplevel;
    wlr_scene_node_set_position(&toplevel->scene_tree->node, cursor->x - grab_x,
                                cursor->y - grab_y);
}

void cursor::Cursor::process_cursor_resize() {
    xdg_shell::Toplevel *toplevel = grabbed_toplevel;
    double border_x = cursor->x - grab_x;
    double border_y = cursor->y - grab_y;
    int new_left = grab_geobox.x;
    int new_right = grab_geobox.x + grab_geobox.width;
    int new_top = grab_geobox.y;
    int new_bottom = grab_geobox.y + grab_geobox.height;

    if(resize_edges & WLR_EDGE_TOP) {
        new_top = border_y;
        if(new_top >= new_bottom) {
            new_top = new_bottom - 1;
        }
    }
    else if(resize_edges & WLR_EDGE_BOTTOM) {
        new_bottom = border_y;
        if(new_bottom <= new_top) {
            new_bottom = new_top + 1;
        }
    }
    if(resize_edges & WLR_EDGE_LEFT) {
        new_left = border_x;
        if(new_left >= new_right) {
            new_left = new_right - 1;
        }
    }
    else if(resize_edges & WLR_EDGE_RIGHT) {
        new_right = border_x;
        if(new_right <= new_left) {
            new_right = new_left + 1;
        }
    }

    struct wlr_box *geo_box = &toplevel->toplevel->base->geometry;
    wlr_scene_node_set_position(&toplevel->scene_tree->node, new_left - geo_box->x,
                                new_top - geo_box->y);

    int new_width = new_right - new_left;
    int new_height = new_bottom - new_top;
    wlr_xdg_toplevel_set_size(toplevel->toplevel, new_width, new_height);
}

void cursor::new_pointer(wlr_input_device *device) {
    wlr_cursor_attach_input_device(Server::instance().cursor.cursor, device);
}

void cursor::cursor_motion(wl_listener *listener, void *data) {
    Cursor *cursor = static_cast<wrapper::Listener<Cursor> *>(listener)->container;
    wlr_pointer_motion_event *event = static_cast<wlr_pointer_motion_event *>(data);

    wlr_cursor_move(cursor->cursor, &event->pointer->base, event->delta_x, event->delta_y);
    cursor->process_motion(event->time_msec);
}

void cursor::cursor_motion_absolute(wl_listener *listener, void *data) {
    Cursor *cursor = static_cast<wrapper::Listener<Cursor> *>(listener)->container;
    wlr_pointer_motion_absolute_event *event =
        static_cast<wlr_pointer_motion_absolute_event *>(data);

    wlr_cursor_warp_absolute(cursor->cursor, &event->pointer->base, event->x, event->y);
    cursor->process_motion(event->time_msec);
}

void cursor::cursor_button(wl_listener *listener, void *data) {
    Cursor *cursor = static_cast<wrapper::Listener<Cursor> *>(listener)->container;
    Server &server = Server::instance();
    wlr_pointer_button_event *event = static_cast<wlr_pointer_button_event *>(data);
    wlr_seat_pointer_notify_button(server.seat.seat, event->time_msec, event->button, event->state);

    if(event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        cursor->reset_cursor_mode();
    }
    else {
        double sx, sy;
        wlr_surface *surface = NULL;

        xdg_shell::Toplevel *toplevel =
            server.toplevel_at(cursor->cursor->x, cursor->cursor->y, surface, sx, sy);
        if(toplevel && surface && surface->mapped) {
            focus_toplevel(toplevel);
            return;
        }

        layer_shell::LayerSurface *layer_surface =
            server.layer_surface_at(cursor->cursor->x, cursor->cursor->y, surface, sx, sy);
        if(layer_surface && surface && surface->mapped) {
            layer_surface->handle_focus();
            return;
        }
    }
}

void cursor::cursor_axis(wl_listener *listener, void *data) {
    Server &server = Server::instance();
    wlr_pointer_axis_event *event = static_cast<wlr_pointer_axis_event *>(data);

    wlr_seat_pointer_notify_axis(server.seat.seat, event->time_msec, event->orientation,
                                 event->delta, event->delta_discrete, event->source,
                                 event->relative_direction);
}

void cursor::cursor_frame(wl_listener *listener, void *data) {
    wlr_seat_pointer_notify_frame(Server::instance().seat.seat);
}

keyboard::Keyboard::Keyboard(wlr_keyboard *kb)
    : keyboard(kb),
      modifiers(this, keyboard::modifiers, &kb->events.modifiers),
      key(this, keyboard::key, &kb->events.key),
      destroy(this, keyboard::destroy, &kb->base.events.destroy) {
    Server &server = Server::instance();
    xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap *keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

    // Assign XKB keymap
    wlr_keyboard_set_keymap(keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(keyboard, 25, 500);

    wlr_seat_set_keyboard(server.seat.seat, keyboard);
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

    wlr_seat_set_keyboard(server.seat.seat, keyboard->keyboard);
    wlr_seat_keyboard_notify_modifiers(server.seat.seat, &keyboard->keyboard->modifiers);
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
        wlr_seat_set_keyboard(server.seat.seat, keyboard->keyboard);
        wlr_seat_keyboard_notify_key(server.seat.seat, event->time_msec, event->keycode,
                                     event->state);
    }
}

void keyboard::destroy(wl_listener *listener, void *data) {
    Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
    Server &server = Server::instance();

    server.keyboards.erase(std::remove(server.keyboards.begin(), server.keyboards.end(), keyboard),
                           server.keyboards.end());

    delete keyboard;
}
