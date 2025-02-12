#pragma once

#include <vector>

#include "input.hpp"
#include "layer-shell.hpp"
#include "output.hpp"
#include "wlr.hpp"
#include "xdg-shell.hpp"

struct Root {
    wlr_scene_tree* shell_background;
    wlr_scene_tree* shell_bottom;
    wlr_scene_tree* shell_top;
    wlr_scene_tree* shell_overlay;

    wlr_scene_tree* floating;
    wlr_scene_tree* fullscreen;
    wlr_scene_tree* popups;
    wlr_scene_tree* seat;

    Root(wlr_scene_tree* parent);

    void arrange();
};

class Server {
    public:
    // Globals
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;

    // Layout/output
    wlr_output_layout* output_layout;

    // scene layout:
    // - root
    //  - layer shell
    //  - tiling
    //  - floating
    //  - fullscreen
    wlr_scene* scene;
    Root root;

    wlr_scene_output_layout* scene_layout;

    // Protocols
    wlr_xdg_shell* xdg_shell;
    wlr_layer_shell_v1* layer_shell;

    cursor::Cursor cursor;

    // Seat
    // TODO: support more seats
    seat::Seat seat;

    // Misc.
    std::vector<output::Output*> outputs;
    std::vector<xdg_shell::Toplevel*> toplevels;
    std::vector<keyboard::Keyboard*> keyboards;

    static Server& instance();
    void start(char* startup_cmd);

    template <typename T>
    T* surface_at(double lx, double ly, wlr_surface*& surface, double& sx, double& sy);

    xdg_shell::Toplevel* toplevel_at(double lx, double ly, wlr_surface*& surface, double& sx,
                                     double& sy);
    layer_shell::LayerSurface* layer_surface_at(double lx, double ly, wlr_surface*& surface,
                                                double& sx, double& sy);

    private:
    // Listeners
    wrapper::Listener<Server> new_output;

    wrapper::Listener<Server> new_xdg_toplevel;
    wrapper::Listener<Server> new_input;

    wrapper::Listener<Server> new_layer_shell_surface;

    Server();
    ~Server();
};
