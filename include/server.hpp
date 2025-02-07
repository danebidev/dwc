#pragma once

#include <vector>

#include "input.hpp"
#include "output.hpp"
#include "toplevel.hpp"
#include "wlr.hpp"
class Server {
    public:
    // Globals
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;

    // Layout/output
    wlr_output_layout* output_layout;
    wlr_scene* scene;
    wlr_scene_output_layout* scene_layout;

    // Protocols
    wlr_xdg_shell* xdg_shell;

    // Cursor
    wlr_cursor* cursor;
    wlr_xcursor_manager* cursor_mgr;
    cursor::CursorMode cursor_mode;

    // Seat
    wlr_seat* seat;

    // Grab
    xdg_shell::Toplevel* grabbed_toplevel;
    double grab_x, grab_y;
    wlr_box grab_geobox;
    uint32_t resize_edges;

    // Misc.
    std::vector<output::Output*> outputs;
    std::vector<xdg_shell::Toplevel*> toplevels;
    std::vector<keyboard::Keyboard*> keyboards;

    static Server& instance();
    void start(char* startup_cmd);

    // Should be called whenever the cursor moves for any reason
    void process_cursor_motion(uint32_t time);

    // "interactive mode" is the mode the compositor is in when pointer
    // events don't get propagated to the client, but are consumed
    // and used for some operation, like move and resize of windows
    void begin_interactive(xdg_shell::Toplevel* toplevel, cursor::CursorMode mode, uint32_t edges);

    private:
    // Listeners
    wrapper::Listener<Server> new_output;
    wrapper::Listener<Server> new_xdg_toplevel;
    wrapper::Listener<Server> new_xdg_popup;

    wrapper::Listener<Server> cursor_motion;
    wrapper::Listener<Server> cursor_motion_absolute;
    wrapper::Listener<Server> cursor_button;
    wrapper::Listener<Server> cursor_axis;
    wrapper::Listener<Server> cursor_frame;

    wrapper::Listener<Server> new_input;
    wrapper::Listener<Server> request_cursor;
    wrapper::Listener<Server> request_set_selection;

    Server();
    ~Server();

    void process_cursor_move();
    void process_cursor_resize();
};
