#pragma once

#include <list>

#include "input.hpp"
#include "layer-shell.hpp"
#include "output.hpp"
#include "root.hpp"
#include "wlr.hpp"
#include "xdg-shell.hpp"

class Server {
    public:
    // Globals
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;

    // Scene graph root
    nodes::Root root;

    wlr_scene_output_layout* scene_layout;

    // Protocols
    wlr_xdg_shell* xdg_shell;
    wlr_layer_shell_v1* layer_shell;

    // Seat
    input::InputManager input_manager;

    // Misc.
    std::list<output::Output*> outputs;
    std::list<xdg_shell::Toplevel*> toplevels;
    std::list<keyboard::Keyboard*> keyboards;

    void start(char* startup_cmd);

    template <typename T>
    T* surface_at(double lx, double ly, wlr_surface*& surface, double& sx, double& sy);

    xdg_shell::Toplevel* toplevel_at(double lx, double ly, wlr_surface*& surface, double& sx,
                                     double& sy);
    layer_shell::LayerSurface* layer_surface_at(double lx, double ly, wlr_surface*& surface,
                                                double& sx, double& sy);

    Server();
    ~Server();

    private:
    // Listeners
    wrapper::Listener<Server> new_output;
    wrapper::Listener<Server> new_xdg_toplevel;
    wrapper::Listener<Server> new_layer_shell_surface;
};

extern Server server;
