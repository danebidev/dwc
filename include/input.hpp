#pragma once

#include "wlr-wrapper.hpp"
#include "wlr.hpp"
#include "xdg-shell.hpp"

namespace seat {
    struct Seat {
        wlr_seat* seat;

        wlr_scene_tree* scene_tree;
        wlr_scene_tree* drag_icons;

        wrapper::Listener<Seat> request_cursor;
        wrapper::Listener<Seat> request_set_selection;

        Seat(const char* seat_name, wl_display* display, wlr_scene_tree* seat_tree);

        void free_listeners();
    };

    void new_input(wl_listener* listener, void* data);
    void request_cursor(wl_listener* listener, void* data);

    // Called by the seat when a client wants to set the selection
    void request_set_selection(wl_listener* listener, void* data);
}

namespace cursor {
    enum class CursorMode { PASSTHROUGH, MOVE, RESIZE };

    struct Cursor {
        wlr_cursor* cursor;
        wlr_xcursor_manager* cursor_mgr;
        cursor::CursorMode cursor_mode;

        // Grab stuff
        xdg_shell::Toplevel* grabbed_toplevel;
        double grab_x, grab_y;
        wlr_box grab_geobox;
        uint32_t resize_edges;

        wrapper::Listener<Cursor> motion;
        wrapper::Listener<Cursor> motion_absolute;
        wrapper::Listener<Cursor> button;
        wrapper::Listener<Cursor> axis;
        wrapper::Listener<Cursor> frame;

        Cursor();
        ~Cursor();

        void reset_cursor_mode();

        // Should be called whenever the cursor moves for any reason
        void process_motion(uint32_t time);

        // "interactive mode" is the mode the compositor is in when pointer
        // events don't get propagated to the client, but are consumed
        // and used for some operation, like move and resize of windows
        void begin_interactive(xdg_shell::Toplevel* toplevel, cursor::CursorMode mode,
                               uint32_t edges);

        void process_cursor_move();
        void process_cursor_resize();
    };

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
