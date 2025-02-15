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
    friend void output::layout_update(wl_listener*, void*);

    public:
    // Globals
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;

    // Scene graph root
    nodes::Root root;

    wlr_scene_output_layout* scene_layout;

    // Input
    input::InputManager input_manager;

    // Misc.
    std::list<xdg_shell::Toplevel*> toplevels;

    Server();
    ~Server();

    void start(char* startup_cmd);

    xdg_shell::Toplevel* toplevel_at(double lx, double ly, wlr_surface*& surface, double& sx,
                                     double& sy);
    layer_shell::LayerSurface* layer_surface_at(double lx, double ly, wlr_surface*& surface,
                                                double& sx, double& sy);

    private:
    // Protocols
    wlr_xdg_shell* xdg_shell;
    wlr_layer_shell_v1* layer_shell;
    wlr_linux_dmabuf_v1* linux_dmabuf_v1;
    wlr_screencopy_manager_v1* screencopy_manager_v1;
    wlr_ext_image_copy_capture_manager_v1* ext_image_copy_capture_manager_v1;
    wlr_output_manager_v1* output_manager_v1;

    template <typename T>
    T* surface_at(double lx, double ly, wlr_surface*& surface, double& sx, double& sy);

    // Listeners
    wrapper::Listener<Server> new_output;
    wrapper::Listener<Server> new_xdg_toplevel;
    wrapper::Listener<Server> new_layer_shell_surface;
    wrapper::Listener<Server> output_test;
    wrapper::Listener<Server> output_apply;

    // Cleanup listeners
    wrapper::Listener<Server> backend_destroy;
    wrapper::Listener<Server> xdg_shell_destroy;
    wrapper::Listener<Server> layer_shell_destroy;
};

extern Server server;
