#pragma once

#include "wlr.hpp"
#include "wlr_wrappers.hpp"

namespace seat {
    void new_input(wl_listener* listener, void* data);
    void request_cursor(wl_listener* listener, void* data);

    // Called by the seat when a client wants to set the selection
    void request_set_selection(wl_listener* listener, void* data);
}

namespace cursor {
    enum class CursorMode { PASSTHROUGH, MOVE, RESIZE };

    void reset_cursor_mode();

    void new_pointer(wlr_input_device* device);

    // Called when a pointer emits a relative pointer motion event
    void cursor_motion(wl_listener* listener, void* data);

    // Called when a pointer emits an absolute pointer motion event
    void cursor_motion_absolute(wl_listener* listener, void* data);

    // Called when a pointer emits a button event
    void cursor_button(wl_listener* listener, void* data);

    // Called when a pointer emits an axis event, like a mouse wheel scroll
    void cursor_axis(wl_listener* listener, void* data);

    // Called when a pointer emits a frame event
    // Frame events are sent after regular pointer events
    // to group multiple events together
    void cursor_frame(wl_listener* listener, void* data);
}

namespace keyboard {
    struct Keyboard {
        wlr_keyboard* keyboard;

        wrapper::Listener<Keyboard> modifiers;
        wrapper::Listener<Keyboard> key;
        wrapper::Listener<Keyboard> destroy;

        Keyboard(wlr_keyboard* kb);
    };

    void new_keyboard(wlr_input_device* device);
    bool handle_keybinding(xkb_keysym_t sym);

    // Called when a modifier key (ctrl, shift, alt, etc.) is pressed
    void modifiers(wl_listener* listener, void* data);

    // Called when a key is pressed or released
    void key(wl_listener* listener, void* data);

    // Called when a keyboard is destroyed
    void destroy(wl_listener* listener, void* data);
}
