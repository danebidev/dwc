#pragma once

#include <list>

#include "input.hpp"
#include "layer-shell.hpp"
#include "output.hpp"
#include "root.hpp"
#include "wlr.hpp"
#include "xdg-shell.hpp"

class Server {
    friend void backend_destroy(wl_listener*, void*);
    friend void xdg_shell_destroy(wl_listener*, void*);
    friend void layer_shell_destroy(wl_listener*, void*);

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

    // Cleanup listeners
    wrapper::Listener<Server> backend_destroy;
    wrapper::Listener<Server> xdg_shell_destroy;
    wrapper::Listener<Server> layer_shell_destroy;
};

void backend_destroy(wl_listener* listener, void* data);
void xdg_shell_destroy(wl_listener* listener, void* data);
void layer_shell_destroy(wl_listener* listener, void* data);

extern Server server;
