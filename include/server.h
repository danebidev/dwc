#pragma once

#include <vector>

#include "input.h"
#include "output.h"
#include "toplevel.h"
#include "wlr.h"
class Server {
    private:
    // Listeners
    wl_listener new_output;
    wl_listener new_xdg_toplevel;
    wl_listener new_xdg_popup;

    wl_listener cursor_motion;
    wl_listener cursor_motion_absolute;
    wl_listener cursor_button;
    wl_listener cursor_axis;
    wl_listener cursor_frame;

    wl_listener new_input;
    wl_listener request_cursor;
    wl_listener request_set_selection;

    // Protocols
    wlr_xdg_shell* xdg_shell;

    Server();
    ~Server();

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

    // Input
    wlr_seat* seat;
    cursor::CursorMode cursor_mode;

    // Cursor
    wlr_cursor* cursor;
    wlr_xcursor_manager* cursor_mgr;

    // Misc.
    std::vector<output::Output*> outputs;
    std::vector<toplevel::Toplevel*> toplevels;
    std::vector<keyboard::Keyboard*> keyboards;

    static Server& instance();
    void start(char* startup_cmd);

    // Should be called whenever the cursor moves for any reason
    void process_cursor_motion(uint32_t time);

    // "interactive mode" is the mode the compositor is in when pointer
    // events don't get propagated to the client, but are consumed
    // and used for some operation, like move and resize of windows
    void begin_interactive(toplevel::Toplevel* toplevel, cursor::CursorMode mode, uint32_t edges);
};
