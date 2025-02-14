#include "input.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <format>

#include "layer-shell.hpp"
#include "server.hpp"
#include "util.hpp"

#define DEFAULT_SEAT "seat0"

cursor::Cursor::Cursor()
    : cursor(wlr_cursor_create()),
      xcursor_mgr(wlr_xcursor_manager_create(nullptr, 24)),
      cursor_mode(CursorMode::PASSTHROUGH),

      motion(this, cursor::cursor_motion, &cursor->events.motion),
      motion_absolute(this, cursor::cursor_motion_absolute, &cursor->events.motion_absolute),
      button(this, cursor::cursor_button, &cursor->events.button),
      axis(this, cursor::cursor_axis, &cursor->events.axis),
      frame(this, cursor::cursor_frame, &cursor->events.frame) {
    wlr_cursor_attach_output_layout(cursor, server.root.output_layout);
}

cursor::Cursor::~Cursor() {
    motion.free();
    motion_absolute.free();
    button.free();
    axis.free();
    frame.free();

    wlr_xcursor_manager_destroy(xcursor_mgr);
    wlr_cursor_destroy(cursor);
}

void cursor::Cursor::reset_cursor_mode() {
    cursor_mode = CursorMode::PASSTHROUGH;
    grabbed_toplevel = nullptr;
}

void cursor::Cursor::set_image(const char *new_image, wl_client *client) {
    if(!(server.input_manager.seat.seat->capabilities & WL_SEAT_CAPABILITY_POINTER))
        return;

    const char *current_image = image;
    image = new_image;
    image_client = client;

    // Unset image if the new image is null
    if(!new_image)
        wlr_cursor_unset_image(cursor);
    // Change image if there was no image before, or the image is different
    else if(!current_image || strcmp(current_image, new_image))
        wlr_cursor_set_xcursor(cursor, xcursor_mgr, new_image);
}

void cursor::Cursor::process_motion(uint32_t time) {
    // If the cursor mode is not passthrough, consume the motion
    if(cursor_mode == cursor::CursorMode::MOVE) {
        process_cursor_move();
        return;
    }
    else if(cursor_mode == cursor::CursorMode::RESIZE) {
        process_cursor_resize();
        return;
    }

    // Surface-local coordinates
    double sx, sy;
    wlr_surface *surface = nullptr;

    // Check if the cursor entered a toplevel surface
    xdg_shell::Toplevel *toplevel = server.toplevel_at(cursor->x, cursor->y, surface, sx, sy);
    if(toplevel) {
        // TODO: set cursor image
        wlr_seat_pointer_notify_enter(server.input_manager.seat.seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server.input_manager.seat.seat, time, sx, sy);
        return;
    }

    // Check if the cursor entered a layer_shell surface
    layer_shell::LayerSurface *layer_surface =
        server.layer_surface_at(cursor->x, cursor->y, surface, sx, sy);
    if(layer_surface) {
        // TODO: set cursor image
        wlr_seat_pointer_notify_enter(server.input_manager.seat.seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server.input_manager.seat.seat, time, sx, sy);
        return;
    }

    // Otherwise, set the default image and clear the focus
    set_image("default", nullptr);
    wlr_seat_pointer_clear_focus(server.input_manager.seat.seat);
}

void cursor::Cursor::begin_interactive(xdg_shell::Toplevel *toplevel, cursor::CursorMode mode,
                                       uint32_t edges) {
    // This should never be called with passthrough mode
    assert(mode != cursor::CursorMode::PASSTHROUGH);

    grabbed_toplevel = toplevel;
    cursor_mode = mode;

    if(mode == cursor::CursorMode::MOVE) {
        // Sets grab coordinates to toplevel-relative coordinates
        grab_x = cursor->x - toplevel->scene_tree->node.x;
        grab_y = cursor->y - toplevel->scene_tree->node.y;
    }
    else {
        // Black magic i don't understand
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
    assert(grabbed_toplevel);
    // Moves the toplevel to the new cursor
    // position, shifting by the grab location
    wlr_scene_node_set_position(&grabbed_toplevel->scene_tree->node, cursor->x - grab_x,
                                cursor->y - grab_y);
}

void cursor::Cursor::process_cursor_resize() {
    xdg_shell::Toplevel *toplevel = grabbed_toplevel;
    // Gets the displacement from the grab location
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
    wlr_cursor_attach_input_device(server.input_manager.seat.cursor.cursor, device);
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
    wlr_pointer_button_event *event = static_cast<wlr_pointer_button_event *>(data);
    wlr_seat_pointer_notify_button(server.input_manager.seat.seat, event->time_msec, event->button,
                                   event->state);

    if(event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
        cursor->reset_cursor_mode();
    }
    else {
        double sx, sy;
        wlr_surface *surface = nullptr;

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
    wlr_pointer_axis_event *event = static_cast<wlr_pointer_axis_event *>(data);

    wlr_seat_pointer_notify_axis(server.input_manager.seat.seat, event->time_msec,
                                 event->orientation, event->delta, event->delta_discrete,
                                 event->source, event->relative_direction);
}

void cursor::cursor_frame(wl_listener *listener, void *data) {
    wlr_seat_pointer_notify_frame(server.input_manager.seat.seat);
}

keyboard::Keyboard::Keyboard(seat::SeatDevice *device)
    : keyboard(wlr_keyboard_from_input_device(device->device->device)),
      seat_dev(device),

      modifiers(this, keyboard::modifiers, &keyboard->events.modifiers),
      key(this, keyboard::key, &keyboard->events.key),
      destroy(this, keyboard::destroy, &keyboard->base.events.destroy) {
    keyboard->data = this;
}

void keyboard::Keyboard::configure() {
    int rate = 25;
    int delay = 500;

    bool repeat_info_changed = rate != repeat_rate || delay != repeat_delay;

    if(repeat_info_changed) {
        repeat_rate = rate;
        repeat_delay = delay;
        wlr_keyboard_set_repeat_info(keyboard, rate, delay);
    }

    xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap *keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

    // Assign XKB keymap
    wlr_keyboard_set_keymap(keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);

    wlr_seat *seat = server.input_manager.seat.seat;
    wlr_keyboard *current_keyboard = wlr_seat_get_keyboard(seat);
    if(!current_keyboard)
        wlr_seat_set_keyboard(seat, keyboard);
}

bool keyboard::handle_keybinding(xkb_keysym_t sym) {
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

    wlr_seat_set_keyboard(server.input_manager.seat.seat, keyboard->keyboard);
    wlr_seat_keyboard_notify_modifiers(server.input_manager.seat.seat,
                                       &keyboard->keyboard->modifiers);
}

void keyboard::key(wl_listener *listener, void *data) {
    Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
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
        wlr_seat_set_keyboard(server.input_manager.seat.seat, keyboard->keyboard);
        wlr_seat_keyboard_notify_key(server.input_manager.seat.seat, event->time_msec,
                                     event->keycode, event->state);
    }
}

void keyboard::destroy(wl_listener *listener, void *data) {
    Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
    server.keyboards.remove(keyboard);
    delete keyboard;
}

seat::SeatDevice::SeatDevice(input::InputDevice *device)
    : device(device),
      keyboard(nullptr) {}

seat::SeatDevice::~SeatDevice() {
    // TODO: destructor
    // This should destroy devices and detach cursor from input devices
}

seat::Seat::Seat(const char *seat_name)
    : seat(wlr_seat_create(server.display, seat_name)),
      scene_tree(wlr_scene_tree_create(server.root.seat)),
      /*drag_icons(wlr_scene_tree_create(scene_tree)),*/

      request_cursor(this, seat::request_cursor, &seat->events.request_set_cursor),
      request_set_selection(this, seat::request_set_selection, &seat->events.request_set_selection),
      destroy(this, seat::destroy, &seat->events.destroy) {
    seat->data = this;
}

void seat::Seat::add_device(input::InputDevice *device) {
    if(get_device(device)) {
        configure_device(get_device(device));
        return;
    }

    SeatDevice *seat_dev = new SeatDevice(device);
    devices.push_back(seat_dev);

    configure_device(seat_dev);
    update_capabilities();
}

void seat::Seat::remove_device(input::InputDevice *device) {
    SeatDevice *seat_dev = get_device(device);
    if(!seat_dev)
        return;

    wlr_log(WLR_DEBUG, "removing device %s from seat %s", device->identifier.c_str(), seat->name);

    devices.remove(seat_dev);
    delete seat_dev;
    update_capabilities();
}

seat::SeatDevice *seat::Seat::get_device(input::InputDevice *device) {
    for(auto &dev : devices) {
        if(device == dev->device)
            return dev;
    }
    return nullptr;
}

void seat::Seat::configure_device(SeatDevice *device) {
    if(!device)
        return;

    switch(device->device->device->type) {
        case WLR_INPUT_DEVICE_POINTER:
            configure_pointer(device);
            break;
        case WLR_INPUT_DEVICE_KEYBOARD:
            configure_keyboard(device);
            break;
        default:
            // TODO
            return;
    }
}

void seat::Seat::configure_xcursor() {
    uint cursor_size = 24;

    setenv("XCURSOR_SIZE", std::to_string(cursor_size).c_str(), true);
    // TODO: load theme from config?
    // setenv("XCURSOR_THEME", "", true);
}

void seat::Seat::configure_pointer(SeatDevice *device) {
    configure_xcursor();
    wlr_cursor_attach_input_device(cursor.cursor, device->device->device);
}

void seat::Seat::configure_keyboard(SeatDevice *device) {
    if(!device->keyboard) {
        device->keyboard = new keyboard::Keyboard(device);
    }
    device->keyboard->configure();

    wlr_keyboard *keyboard = device->keyboard->keyboard;
    wlr_keyboard *current_keyboard = wlr_seat_get_keyboard(seat);
    if(current_keyboard != keyboard)
        return;

    wlr_surface *surface = seat->keyboard_state.focused_surface;
    if(surface) {
        keyboard_notify_enter(surface);
    }
}

void seat::Seat::keyboard_notify_enter(wlr_surface *surface) {
    wlr_keyboard *kb = wlr_seat_get_keyboard(seat);
    if(!kb) {
        wlr_seat_keyboard_notify_enter(seat, surface, nullptr, 0, nullptr);
        return;
    }

    keyboard::Keyboard *keyboard = static_cast<keyboard::Keyboard *>(kb->data);
    assert(keyboard);

    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keyboard->keycodes,
                                   keyboard->keyboard->num_keycodes,
                                   &keyboard->keyboard->modifiers);
}

void seat::Seat::update_capabilities() {
    uint32_t caps = 0;
    uint32_t prev_caps = seat->capabilities;

    for(const auto &dev : devices) {
        switch(dev->device->device->type) {
            case WLR_INPUT_DEVICE_POINTER:
                caps |= WL_SEAT_CAPABILITY_POINTER;
                break;
            case WLR_INPUT_DEVICE_KEYBOARD:
                caps |= WL_SEAT_CAPABILITY_KEYBOARD;
                break;
            default:
                // TODO
                break;
        }
    }

    if(!(caps & WL_SEAT_CAPABILITY_POINTER)) {
        cursor.set_image(nullptr, nullptr);
        wlr_seat_set_capabilities(seat, caps);
    }
    else {
        wlr_seat_set_capabilities(seat, caps);
        if(!(prev_caps & WL_SEAT_CAPABILITY_POINTER)) {
            cursor.set_image("default", nullptr);
        }
    }
}

void seat::request_cursor(wl_listener *listener, void *data) {
    Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;
    wlr_seat_pointer_request_set_cursor_event *event =
        static_cast<wlr_seat_pointer_request_set_cursor_event *>(data);

    if(seat->cursor.cursor_mode != cursor::CursorMode::PASSTHROUGH)
        return;

    // This event can be sent by any client so we check
    // that it's actually being sent by the focused client
    wlr_seat_client *focused_client = seat->seat->pointer_state.focused_client;
    if(focused_client == event->seat_client)
        wlr_cursor_set_surface(seat->cursor.cursor, event->surface, event->hotspot_x,
                               event->hotspot_y);
}

void seat::request_set_selection(wl_listener *listener, void *data) {
    Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;
    wlr_seat_request_set_selection_event *event =
        static_cast<wlr_seat_request_set_selection_event *>(data);

    wlr_seat_set_selection(seat->seat, event->source, event->serial);
}

void seat::destroy(wl_listener *listener, void *data) {
    Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;

    seat->request_cursor.free();
    seat->request_set_selection.free();
    seat->destroy.free();
}

input::InputDevice::InputDevice(wlr_input_device *device)
    : device(device),
      identifier(device_identifier(device)),
      destroy(this, input::device_destroy, &device->events.destroy) {
    device->data = this;
}

input::InputManager::InputManager(wl_display *display, wlr_backend *backend)
    : seat(DEFAULT_SEAT),
      new_input(this, input::new_input, &backend->events.new_input),
      backend_destroy(this, input::backend_destroy, &backend->events.destroy) {}

void input::new_input(wl_listener *listener, void *data) {
    InputManager *input_manager =
        static_cast<wrapper::Listener<InputManager> *>(listener)->container;
    wlr_input_device *device = static_cast<wlr_input_device *>(data);

    InputDevice *input_dev = new InputDevice(device);
    input_manager->devices.push_back(input_dev);

    wlr_log(WLR_DEBUG, "adding device: '%s'", input_dev->identifier.c_str());

    input_manager->seat.add_device(input_dev);
}

void input::device_destroy(wl_listener *listener, void *data) {
    InputDevice *input_dev = static_cast<wrapper::Listener<InputDevice> *>(listener)->container;

    wlr_log(WLR_DEBUG, "removing device: %s", input_dev->identifier.c_str());

    server.input_manager.seat.remove_device(input_dev);
    server.input_manager.devices.remove(input_dev);
    delete input_dev;
}

std::string input::device_identifier(wlr_input_device *device) {
    int vendor = 0, product = 0;
    if(wlr_input_device_is_libinput(device)) {
        libinput_device *libinput_dev = wlr_libinput_get_device_handle(device);
        vendor = libinput_device_get_id_vendor(libinput_dev);
        product = libinput_device_get_id_product(libinput_dev);
    }

    // Apparently the device name can be null (how??)
    std::string name = device->name ? device->name : "";
    trim(name);

    for(auto &c : name) {
        // Device names can contain not-printable characters
        if(c == ' ' || !isprint(c))
            c = '_';
    }

    return std::format("{}:{}:{}", vendor, product, name);
}

void input::backend_destroy(wl_listener *listener, void *data) {
    InputManager *input = static_cast<wrapper::Listener<InputManager> *>(listener)->container;
    input->new_input.free();
    input->backend_destroy.free();
}
