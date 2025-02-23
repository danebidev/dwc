#include "input.hpp"

#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <format>

#include "layer-shell.hpp"
#include "server.hpp"
#include "util.hpp"

#define DEFAULT_SEAT "seat0"

namespace cursor {
    // Called when a pointer emits a relative pointer motion event
    void motion(wl_listener *listener, void *data) {
        Cursor *cursor = static_cast<wrapper::Listener<Cursor> *>(listener)->container;
        wlr_pointer_motion_event *event = static_cast<wlr_pointer_motion_event *>(data);

        wlr_cursor_move(cursor->cursor, &event->pointer->base, event->delta_x, event->delta_y);
        cursor->process_motion(event->time_msec);
    }

    // Called when a pointer emits an absolute pointer motion event
    void motion_absolute(wl_listener *listener, void *data) {
        Cursor *cursor = static_cast<wrapper::Listener<Cursor> *>(listener)->container;
        wlr_pointer_motion_absolute_event *event =
            static_cast<wlr_pointer_motion_absolute_event *>(data);

        wlr_cursor_warp_absolute(cursor->cursor, &event->pointer->base, event->x, event->y);
        cursor->process_motion(event->time_msec);
    }

    // Called when a pointer emits a button event
    void button(wl_listener *listener, void *data) {
        Cursor *cursor = static_cast<wrapper::Listener<Cursor> *>(listener)->container;
        wlr_pointer_button_event *event = static_cast<wlr_pointer_button_event *>(data);
        wlr_seat_pointer_notify_button(server.input_manager.seat.seat, event->time_msec,
                                       event->button, event->state);

        if(event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
            cursor->reset_cursor_mode();
        }
        else {
            double sx, sy;
            wlr_surface *surface = nullptr;

            xdg_shell::Toplevel *toplevel =
                server.toplevel_at(cursor->cursor->x, cursor->cursor->y, surface, sx, sy);
            if(toplevel && surface && surface->mapped) {
                server.input_manager.seat.focus_node(&toplevel->node);
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

    // Called when a pointer emits an axis event, like a mouse wheel scroll
    void axis(wl_listener *listener, void *data) {
        wlr_pointer_axis_event *event = static_cast<wlr_pointer_axis_event *>(data);

        wlr_seat_pointer_notify_axis(server.input_manager.seat.seat, event->time_msec,
                                     event->orientation, event->delta, event->delta_discrete,
                                     event->source, event->relative_direction);
    }

    // Called when a pointer emits a frame event
    // Frame events are sent after regular pointer events
    // to group multiple events together
    void frame(wl_listener *listener, void *data) {
        wlr_seat_pointer_notify_frame(server.input_manager.seat.seat);
    }

    Cursor::Cursor()
        : cursor(wlr_cursor_create()),
          cursor_mode(CursorMode::PASSTHROUGH),
          xcursor_mgr(wlr_xcursor_manager_create("default", 24)),

          motion(this, cursor::motion, &cursor->events.motion),
          motion_absolute(this, cursor::motion_absolute, &cursor->events.motion_absolute),
          button(this, cursor::button, &cursor->events.button),
          axis(this, cursor::axis, &cursor->events.axis),
          frame(this, cursor::frame, &cursor->events.frame) {
        wlr_cursor_attach_output_layout(cursor, server.root.output_layout);
    }

    Cursor::~Cursor() {
        motion.free();
        motion_absolute.free();
        button.free();
        axis.free();
        frame.free();

        wlr_xcursor_manager_destroy(xcursor_mgr);
        wlr_cursor_destroy(cursor);
    }

    void Cursor::reset_cursor_mode() {
        cursor_mode = CursorMode::PASSTHROUGH;
        grabbed_toplevel = nullptr;
    }

    void Cursor::set_image(const char *new_image) {
        if(!(server.input_manager.seat.seat->capabilities & WL_SEAT_CAPABILITY_POINTER))
            return;

        // Unset image if the new image is null
        if(!new_image)
            wlr_cursor_unset_image(cursor);
        else
            wlr_cursor_set_xcursor(cursor, xcursor_mgr, new_image);
    }

    void Cursor::process_motion(uint32_t time) {
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
            wlr_seat_pointer_notify_enter(server.input_manager.seat.seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(server.input_manager.seat.seat, time, sx, sy);
            server.input_manager.seat.focus_node(&toplevel->node);
            return;
        }

        // Check if the cursor entered a layer_shell surface
        layer_shell::LayerSurface *layer_surface =
            server.layer_surface_at(cursor->x, cursor->y, surface, sx, sy);
        if(layer_surface) {
            // TODO: set cursor image
            wlr_seat_pointer_notify_enter(server.input_manager.seat.seat, surface, sx, sy);
            wlr_seat_pointer_notify_motion(server.input_manager.seat.seat, time, sx, sy);
            server.input_manager.seat.focus_node(&layer_surface->node);
            return;
        }

        // Otherwise, set the default image and clear the focus
        set_image("default");
        wlr_seat_pointer_clear_focus(server.input_manager.seat.seat);
    }

    void Cursor::begin_interactive(xdg_shell::Toplevel *toplevel, cursor::CursorMode mode,
                                   uint32_t edges) {
        // This should never be called with passthrough mode
        assert(mode != cursor::CursorMode::PASSTHROUGH);

        grabbed_toplevel = toplevel;
        cursor_mode = mode;

        if(mode == CursorMode::MOVE) {
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

    void Cursor::move_to_coords(double x, double y, wlr_input_device *dev) {
        wlr_box box;
        wlr_output_layout_get_box(server.root.output_layout, nullptr, &box);
        wlr_cursor_warp_absolute(cursor, dev, x / box.width, y / box.height);
    }

    void Cursor::process_cursor_move() {
        assert(grabbed_toplevel);
        int x = cursor->x - grab_x;
        int y = cursor->y - grab_y;

        wlr_scene_node_set_position(&grabbed_toplevel->scene_tree->node, x, y);

        output::Output *output = server.output_manager.output_at(x, y);
        if(output->active_workspace != grabbed_toplevel->workspace) {
            if(grabbed_toplevel->workspace->last_focused_toplevel == grabbed_toplevel)
                grabbed_toplevel->workspace->last_focused_toplevel = nullptr;

            grabbed_toplevel->workspace->floating.remove(grabbed_toplevel);
            grabbed_toplevel->workspace = output->active_workspace;

            output->active_workspace->floating.push_back(grabbed_toplevel);
        }
    }

    void Cursor::process_cursor_resize() {
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

        wlr_box *geo_box = &toplevel->toplevel->base->geometry;
        wlr_scene_node_set_position(&toplevel->scene_tree->node, new_left - geo_box->x,
                                    new_top - geo_box->y);

        int new_width = new_right - new_left;
        int new_height = new_bottom - new_top;
        wlr_xdg_toplevel_set_size(toplevel->toplevel, new_width, new_height);
    }
}

namespace keyboard {
    bool handle_keybind(const config::Bind &bind) {
        for(auto &[cur_bind, command] : conf.binds) {
            if(cur_bind == bind) {
                command->execute(ConfigLoadPhase::BIND);
                return true;
            }
        }

        return false;
    }

    // Called when a modifier key (ctrl, shift, alt, etc.) is pressed
    void modifiers(wl_listener *listener, void *data) {
        Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;

        wlr_seat_set_keyboard(server.input_manager.seat.seat, keyboard->keyboard);
        wlr_seat_keyboard_notify_modifiers(server.input_manager.seat.seat,
                                           &keyboard->keyboard->modifiers);
    }

    // Called when a key is pressed or released
    void key(wl_listener *listener, void *data) {
        Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
        wlr_keyboard_key_event *event = static_cast<wlr_keyboard_key_event *>(data);

        // libinput keycode -> xkbcommon
        uint32_t keycode = event->keycode + 8;

        bool handled = false;

        if(event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            // Get pressed modifiers
            uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->keyboard);

            const xkb_keysym_t *raw_syms, *translated_syms;

            // Raw keybinds
            size_t nsyms = keyboard->keysyms_raw(keycode, &raw_syms);
            for(size_t i = 0; i < nsyms; i++) {
                handled |= handle_keybind(config::Bind { modifiers, raw_syms[i] });
                if(handled)
                    goto end;
            }

            handled |= keyboard->exec_compositor_binding(raw_syms, modifiers, nsyms);
            if(handled)
                goto end;

            // Translated keybinds
            nsyms = keyboard->keysyms_translated(keycode, &translated_syms, &modifiers);
            if((modifiers & WLR_MODIFIER_SHIFT) || (modifiers & WLR_MODIFIER_CAPS)) {
                for(size_t i = 0; i < nsyms; i++) {
                    handled |= handle_keybind(config::Bind { modifiers, translated_syms[i] });
                    if(handled)
                        goto end;
                }
            }
            handled |= keyboard->exec_compositor_binding(translated_syms, modifiers, nsyms);
        }

    end:
        if(!handled) {
            wlr_seat_set_keyboard(server.input_manager.seat.seat, keyboard->keyboard);
            wlr_seat_keyboard_notify_key(server.input_manager.seat.seat, event->time_msec,
                                         event->keycode, event->state);
        }
    }

    // Called when a keyboard is destroyed
    void destroy(wl_listener *listener, void *data) {
        Keyboard *keyboard = static_cast<wrapper::Listener<Keyboard> *>(listener)->container;
        delete keyboard;
    }

    Keyboard::Keyboard(seat::SeatDevice *device)
        : keyboard(wlr_keyboard_from_input_device(device->device->device)),
          seat_dev(device),

          modifiers(this, keyboard::modifiers, &keyboard->events.modifiers),
          key(this, keyboard::key, &keyboard->events.key),
          destroy(this, keyboard::destroy, &keyboard->base.events.destroy) {
        keyboard->data = this;
    }

    void Keyboard::configure() {
        int rate = 25;
        int delay = 500;

        bool repeat_info_changed = rate != repeat_rate || delay != repeat_delay;

        if(repeat_info_changed) {
            repeat_rate = rate;
            repeat_delay = delay;
            wlr_keyboard_set_repeat_info(keyboard, rate, delay);
        }

        xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_keymap *keymap =
            xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

        // Assign XKB keymap
        wlr_keyboard_set_keymap(keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);

        wlr_seat *seat = server.input_manager.seat.seat;
        wlr_keyboard *current_keyboard = wlr_seat_get_keyboard(seat);
        if(!current_keyboard)
            wlr_seat_set_keyboard(seat, keyboard);
    }

    uint32_t Keyboard::keysyms_raw(xkb_keycode_t keycode, const xkb_keysym_t **keysyms) {
        xkb_layout_index_t layout = xkb_state_key_get_layout(keyboard->xkb_state, keycode);
        return xkb_keymap_key_get_syms_by_level(keyboard->keymap, keycode, layout, 0, keysyms);
    }

    uint32_t Keyboard::keysyms_translated(xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
                                          uint32_t *modifiers) {
        xkb_mod_mask_t consumed =
            xkb_state_key_get_consumed_mods2(keyboard->xkb_state, keycode, XKB_CONSUMED_MODE_XKB);
        *modifiers &= ~consumed;
        return xkb_state_key_get_syms(keyboard->xkb_state, keycode, keysyms);
    }

    bool Keyboard::exec_compositor_binding(const xkb_keysym_t *pressed_keysyms, uint32_t modifiers,
                                           size_t keysyms_len) {
        for(size_t i = 0; i < keysyms_len; ++i) {
            xkb_keysym_t keysym = pressed_keysyms[i];

            if(keysym >= XKB_KEY_XF86Switch_VT_1 && keysym <= XKB_KEY_XF86Switch_VT_12) {
                if(server.session) {
                    unsigned vt = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
                    wlr_session_change_vt(server.session, vt);
                }
                return true;
            }
        }

        return false;
    }
}

namespace seat {
    // Called when a seat node is destroyed
    void seat_node_destroy(wl_listener *listener, void *data) {
        SeatNode *seat_node = static_cast<wrapper::Listener<SeatNode> *>(listener)->container;
        Seat *seat = seat_node->seat;

        bool had_focus = false;
        bool exclusivity = seat_node->node->has_exclusivity();

        if(seat->previous_toplevel == seat_node)
            seat->previous_toplevel = nullptr;

        std::list<SeatNode *> &stack = exclusivity ? seat->exclusivity_stack : seat->focus_stack;
        if(!stack.empty() && stack.front() == seat_node)
            had_focus = true;

        delete seat_node;

        if(!had_focus)
            return;

        SeatNode *focus = seat->get_next_focus();
        if(!focus)
            return;

        seat->focus_node(focus->node);
    }

    // Called by the root when a new node is added to the tree
    void new_node(wl_listener *listener, void *data) {
        Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;
        nodes::Node *node = static_cast<nodes::Node *>(data);
        SeatNode *seat_node = seat->get_seat_node(node);

        std::list<SeatNode *> &stack =
            node->has_exclusivity() ? seat->exclusivity_stack : seat->focus_stack;
        stack.push_front(seat_node);
        seat->focus_node(node);
    }

    // Called by the seat when a client wants to set the cursor image
    void request_cursor(wl_listener *listener, void *data) {
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

    // Called by the seat when a client wants to set the selection
    void request_set_selection(wl_listener *listener, void *data) {
        Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;
        wlr_seat_request_set_selection_event *event =
            static_cast<wlr_seat_request_set_selection_event *>(data);

        wlr_seat_set_selection(seat->seat, event->source, event->serial);
    }

    // Called when a seat is destroyed
    void destroy(wl_listener *listener, void *data) {
        Seat *seat = static_cast<wrapper::Listener<Seat> *>(listener)->container;

        seat->request_cursor.free();
        seat->request_set_selection.free();
        seat->destroy.free();
    }

    SeatDevice::SeatDevice(input::InputDevice *device)
        : device(device),
          keyboard(nullptr) {}

    SeatNode::SeatNode(nodes::Node *node, seat::Seat *seat)
        : node(node),
          seat(seat),

          destroy(this, seat::seat_node_destroy, &node->events.node_destroy) {}

    SeatNode::~SeatNode() {
        std::list<SeatNode *> &stack =
            node->has_exclusivity() ? seat->exclusivity_stack : seat->focus_stack;

        stack.remove(this);
        seat->focused_node = nullptr;
    }

    SeatDevice::~SeatDevice() {
        // TODO: destructor
        // This should destroy devices and detach cursor from input devices
    }

    Seat::Seat(const char *seat_name)
        : seat(wlr_seat_create(server.display, seat_name)),
          previous_toplevel(nullptr),
          scene_tree(wlr_scene_tree_create(server.root.seat)),
          /*drag_icons(wlr_scene_tree_create(scene_tree)),*/

          new_node(this, seat::new_node, &server.root.events.new_node),
          request_cursor(this, seat::request_cursor, &seat->events.request_set_cursor),
          request_set_selection(this, seat::request_set_selection,
                                &seat->events.request_set_selection),
          destroy(this, seat::destroy, &seat->events.destroy) {
        seat->data = this;
    }

    void Seat::add_device(input::InputDevice *device) {
        if(get_device(device)) {
            configure_device(get_device(device));
            return;
        }

        SeatDevice *seat_dev = new SeatDevice(device);
        devices.push_back(seat_dev);

        configure_device(seat_dev);
        update_capabilities();
    }

    void Seat::remove_device(input::InputDevice *device) {
        SeatDevice *seat_dev = get_device(device);
        if(!seat_dev)
            return;

        wlr_log(WLR_DEBUG, "removing device %s from seat %s", device->identifier.c_str(),
                seat->name);

        devices.remove(seat_dev);
        delete seat_dev;
        update_capabilities();
    }

    seat::SeatNode *seat::Seat::get_seat_node(nodes::Node *node) {
        for(auto &seat_node : (node->has_exclusivity() ? exclusivity_stack : focus_stack)) {
            if(seat_node->node == node)
                return seat_node;
        }

        SeatNode *seat_node = new SeatNode(node, this);
        return seat_node;
    }

    SeatNode *seat::Seat::get_next_focus() {
        if(!exclusivity_stack.empty())
            return exclusivity_stack.front();

        if(!focus_stack.empty())
            return focus_stack.front();

        return nullptr;
    }

    void Seat::focus_node(nodes::Node *node) {
        if(focused_node && focused_node->node->has_exclusivity())
            return;

        if(!node) {
            focus_surface(nullptr, false);
            return;
        }

        SeatNode *seat_node = get_seat_node(node);

        if(focused_node && focused_node == seat_node)
            return;

        std::list<SeatNode *> &stack = node->has_exclusivity() ? exclusivity_stack : focus_stack;
        if(stack.remove(seat_node))
            stack.push_front(seat_node);

        if(node->type == nodes::NodeType::LAYER_SURFACE) {
            focus_layer(node->val.layer_surface->layer_surface);
            focused_node = seat_node;
            return;
        }

        previous_toplevel = seat_node;

        focus_surface(node->val.toplevel->toplevel->base->surface, true);
        focused_node = seat_node;

        workspace::Workspace *ws = node->val.toplevel->workspace;
        if(ws)
            ws->last_focused_toplevel = node->val.toplevel;

        update_toplevel_activation(node, true);
    }

    void Seat::focus_surface(wlr_surface *surface, bool toplevel) {
        if(focused_node && focused_node->node->type == nodes::NodeType::TOPLEVEL) {
            if(!surface || toplevel)
                update_toplevel_activation(focused_node->node, false);
        }

        if(!surface) {
            if(previous_toplevel &&
               previous_toplevel->node->val.toplevel->toplevel->base->surface->mapped) {
                wlr_seat_keyboard_notify_clear_focus(seat);
                focus_node(previous_toplevel->node);
                previous_toplevel = nullptr;
                return;
            }
            wlr_seat_keyboard_notify_clear_focus(seat);
            focused_node = nullptr;
            return;
        }

        keyboard_notify_enter(surface);
    }

    void Seat::focus_layer(wlr_layer_surface_v1 *layer) {
        if(!layer) {
            focus_surface(nullptr, false);
            return;
        }
        assert(layer->surface->mapped);

        if(!focused_node) {
            focus_surface(layer->surface, false);
            return;
        }

        SeatNode *previous_focus = focused_node;

        if(focused_node->node->type != nodes::NodeType::LAYER_SURFACE)
            focus_surface(layer->surface, false);

        if(focused_node->node->type == nodes::NodeType::LAYER_SURFACE &&
           focused_node->node->val.layer_surface->layer_surface == layer)
            return;

        if(!exclusivity_stack.empty() &&
           layer->current.layer <
               focused_node->node->val.layer_surface->layer_surface->current.layer)
            return;

        focus_surface(layer->surface, false);

        if(previous_focus && previous_focus->node->type == nodes::NodeType::TOPLEVEL)
            update_toplevel_activation(previous_focus->node, true);
        else if(previous_toplevel && previous_toplevel->node)
            update_toplevel_activation(previous_toplevel->node, true);
    }

    SeatDevice *seat::Seat::get_device(input::InputDevice *device) {
        for(auto &dev : devices) {
            if(device == dev->device)
                return dev;
        }
        return nullptr;
    }

    void Seat::configure_device(SeatDevice *device) {
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

    void Seat::configure_xcursor() {
        uint cursor_size = 24;

        setenv("XCURSOR_SIZE", std::to_string(cursor_size).c_str(), true);
        // TODO: load theme from config?
        // setenv("XCURSOR_THEME", "", true);
    }

    void Seat::configure_pointer(SeatDevice *device) {
        configure_xcursor();
        wlr_cursor_attach_input_device(cursor.cursor, device->device->device);
    }

    void Seat::configure_keyboard(SeatDevice *device) {
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

    void Seat::keyboard_notify_enter(wlr_surface *surface) {
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

    void Seat::update_toplevel_activation(nodes::Node *node, bool activate) {
        if(node && node->type == nodes::NodeType::TOPLEVEL) {
            wlr_xdg_toplevel_set_activated(node->val.toplevel->toplevel, activate);
            if(activate)
                wlr_scene_node_raise_to_top(&node->val.toplevel->scene_tree->node);
        }
    }

    void Seat::update_capabilities() {
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
            cursor.set_image(nullptr);
            wlr_seat_set_capabilities(seat, caps);
        }
        else {
            wlr_seat_set_capabilities(seat, caps);
            if(!(prev_caps & WL_SEAT_CAPABILITY_POINTER)) {
                cursor.set_image("default");
            }
        }
    }

}

namespace input {
    void backend_destroy(wl_listener *listener, void *data) {
        InputManager *input = static_cast<wrapper::Listener<InputManager> *>(listener)->container;
        input->new_input.free();
        input->backend_destroy.free();
    }

    // Called when a new input is made available by the backend
    void new_input(wl_listener *listener, void *data) {
        InputManager *input_manager =
            static_cast<wrapper::Listener<InputManager> *>(listener)->container;
        wlr_input_device *device = static_cast<wlr_input_device *>(data);

        InputDevice *input_dev = new InputDevice(device);
        input_manager->devices.push_back(input_dev);

        wlr_log(WLR_DEBUG, "adding device: '%s'", input_dev->identifier.c_str());

        input_manager->seat.add_device(input_dev);
    }

    // Called when a device of any kind is destroyed
    void device_destroy(wl_listener *listener, void *data) {
        InputDevice *input_dev = static_cast<wrapper::Listener<InputDevice> *>(listener)->container;

        wlr_log(WLR_DEBUG, "removing device: %s", input_dev->identifier.c_str());

        server.input_manager.seat.remove_device(input_dev);
        server.input_manager.devices.remove(input_dev);
        delete input_dev;
    }

    InputDevice::InputDevice(wlr_input_device *device)
        : device(device),
          identifier(device_identifier(device)),
          destroy(this, input::device_destroy, &device->events.destroy) {
        device->data = this;
    }

    InputManager::InputManager(wl_display *display, wlr_backend *backend)
        : seat(DEFAULT_SEAT),
          new_input(this, input::new_input, &backend->events.new_input),
          backend_destroy(this, input::backend_destroy, &backend->events.destroy) {}
}
