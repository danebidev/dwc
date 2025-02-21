#pragma once

#include <list>
#include <string>

#include "config/config.hpp"
#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"
#include "xdg-shell.hpp"

namespace input {
    class InputDevice;
};

namespace seat {
    class SeatDevice;
    class Seat;
}

namespace cursor {
    enum class CursorMode { PASSTHROUGH, MOVE, RESIZE };

    class Cursor {
        friend void motion(wl_listener*, void*);
        friend void motion_absolute(wl_listener*, void*);
        friend void button(wl_listener*, void*);
        friend void axis(wl_listener*, void*);
        friend void frame(wl_listener*, void*);

        public:
        // wlr utility to manage the cursor image
        // Can attach multiple input devices to it
        wlr_cursor* cursor;
        cursor::CursorMode cursor_mode;

        xdg_shell::Toplevel* grabbed_toplevel;

        Cursor();
        ~Cursor();

        // Sets the cursor image, null image unsets the image
        void set_image(const char* image);

        // Resets cursor mode to passthrough
        void reset_cursor_mode();

        // "interactive mode" is the mode the compositor is in when pointer
        // events don't get propagated to the client, but are consumed
        // and used for some operation, like move and resize of windows
        // edges is ignored if the operation is move
        void begin_interactive(xdg_shell::Toplevel* toplevel, cursor::CursorMode mode,
                               uint32_t edges);

        private:
        // Manager for the cursor image theme
        wlr_xcursor_manager* xcursor_mgr;

        // Currently grabbed toplevel, or null if none
        double grab_x, grab_y;
        wlr_box grab_geobox;
        uint32_t resize_edges;

        wrapper::Listener<Cursor> motion;
        wrapper::Listener<Cursor> motion_absolute;
        wrapper::Listener<Cursor> button;
        wrapper::Listener<Cursor> axis;
        wrapper::Listener<Cursor> frame;

        // Should be called whenever the cursor moves for any reason
        void process_motion(uint32_t time);
        // Handles toplevel movement
        void process_cursor_move();
        // Handles toplevel resize
        void process_cursor_resize();
    };
}

namespace keyboard {
    class Keyboard {
        friend void modifiers(wl_listener*, void*);
        friend void key(wl_listener*, void*);
        friend void destroy(wl_listener*, void*);

        public:
        wlr_keyboard* keyboard;

        Keyboard(seat::SeatDevice* keyboard);

        // Configure keyboard repeat rate, keymap, and set the keyboard in the seat
        void configure();

        private:
        seat::SeatDevice* seat_dev;

        // TODO: set these from config
        int repeat_rate;
        int repeat_delay;

        wrapper::Listener<Keyboard> modifiers;
        wrapper::Listener<Keyboard> key;
        wrapper::Listener<Keyboard> destroy;

        uint32_t keysyms_raw(xkb_keycode_t keycode, const xkb_keysym_t** keysyms);
        uint32_t keysyms_translated(xkb_keycode_t keycode, const xkb_keysym_t** keysyms,
                                    uint32_t* modifiers);
    };
}

namespace seat {
    class SeatNode {
        friend void seat_node_destroy(wl_listener*, void*);

        public:
        nodes::Node* node;

        SeatNode(nodes::Node* node, seat::Seat* seat);
        ~SeatNode();

        private:
        // Keep track of the seat, so we can update the focus stack
        seat::Seat* seat;

        wrapper::Listener<SeatNode> destroy;
    };

    class SeatDevice {
        public:
        input::InputDevice* device;

        keyboard::Keyboard* keyboard;

        SeatDevice(input::InputDevice* device);
        ~SeatDevice();
    };

    class Seat {
        friend void new_node(wl_listener*, void*);
        friend void request_cursor(wl_listener* listener, void* data);
        friend void request_set_selection(wl_listener* listener, void* data);
        friend void destroy(wl_listener* listener, void* data);

        public:
        wlr_seat* seat;
        cursor::Cursor cursor;

        // List of nodes in focus stack
        std::list<SeatNode*> focus_stack;
        std::list<SeatNode*> exclusivity_stack;

        // Currently focused node
        SeatNode* focused_node;

        seat::SeatNode* previous_toplevel;

        Seat(const char* seat_name);

        void focus_node(nodes::Node* node);
        SeatNode* get_next_focus();

        // Adds the device to the seat
        void add_device(input::InputDevice* device);
        // Removes the device from the seat
        void remove_device(input::InputDevice* device);

        private:
        wlr_scene_tree* scene_tree;
        /*wlr_scene_tree* drag_icons;*/

        wrapper::Listener<Seat> new_node;
        wrapper::Listener<Seat> request_cursor;
        wrapper::Listener<Seat> request_set_selection;

        wrapper::Listener<Seat> destroy;

        // Gets or creates a new seat node from the root node
        SeatNode* get_seat_node(nodes::Node* node);

        void focus_surface(wlr_surface* surface, bool toplevel);
        void focus_layer(wlr_layer_surface_v1* layer);

        // List of devices attached to this seat
        // Since we currently only have a single seat, this
        // this match the input manager device list
        std::list<SeatDevice*> devices;

        // Returns a seat device from the InputDevice, or nullptr if it can't be found
        SeatDevice* get_device(input::InputDevice* device);

        // Various configuration functions

        // This just gets a generic seat device and calls the right configure function
        void configure_device(SeatDevice* device);
        void configure_pointer(SeatDevice* device);
        void configure_keyboard(SeatDevice* device);

        // Configure xcursor themes and size
        void configure_xcursor();
        // Notifies the seat that the keyboard focus has changed
        void keyboard_notify_enter(wlr_surface* surface);

        void update_toplevel_activation(nodes::Node* node, bool activated);

        // Updates capabilities based on current seat devices
        // Should be called whenever seat devices change
        void update_capabilities();
    };

}

namespace input {
    void backend_destroy(wl_listener* listener, void* data);

    // A wrapper around a generic input device to allow for automatic
    // destruction and better manage interaction with the seat
    class InputDevice {
        public:
        wlr_input_device* device;

        // Identifier that includes vendor and product id
        // This should match sway's identifier style
        std::string identifier;

        InputDevice(wlr_input_device* device);

        private:
        wrapper::Listener<InputDevice> destroy;
    };

    class InputManager {
        friend void input::backend_destroy(wl_listener*, void*);

        public:
        // Multi-seat support is pain, so it's currently single-seat
        seat::Seat seat;

        // Trackes all input devices that the compositor is currently aware of
        std::list<InputDevice*> devices;

        InputManager(wl_display* display, wlr_backend* backend);

        private:
        wrapper::Listener<InputManager> new_input;
        wrapper::Listener<InputManager> backend_destroy;
    };
}
