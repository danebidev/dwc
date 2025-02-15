#include "server.hpp"

#include <unistd.h>

#include <cassert>
#include <stdexcept>

#include "layer-shell.hpp"
#include "output.hpp"
#include "wlr.hpp"

Server server;

Server::Server()
    :  // wl_display global.
       // Needed for the registry and the creation of more objects
      display(wl_display_create()),

      // Backend. Abstracts input and output hardware.
      // Supports stuff like X11 windows, DRM, libinput, headless, etc.
      backend(wlr_backend_autocreate(wl_display_get_event_loop(display), nullptr)),

      // Automatically chooses a wlr renderer. Can be Pixman, GLES2 or Vulkan.
      // Can be overridden by the user with the WLR_RENDERER env var
      renderer(wlr_renderer_autocreate(backend)),

      // Creates an allocator. The allocator allocates pixel buffers with
      // the correct capabilities and position based on the backend and renderer
      allocator(wlr_allocator_autocreate(backend, renderer)),

      // Root of the scene graph tree
      root(display),

      // Attaches an output layout to a scene, to synchronize the positions of scene
      // outputs with the positions of corresponding layout outputs
      scene_layout(wlr_scene_attach_output_layout(root.scene, root.output_layout)),

      // Protocols
      xdg_shell(wlr_xdg_shell_create(display, 6)),
      layer_shell(wlr_layer_shell_v1_create(display, 5)),
      screencopy_manager_v1(wlr_screencopy_manager_v1_create(display)),

      input_manager(display, backend),

      // Listeners
      new_output(this, output::new_output, &backend->events.new_output),
      new_xdg_toplevel(this, xdg_shell::new_xdg_toplevel, &xdg_shell->events.new_toplevel),
      new_layer_shell_surface(this, layer_shell::new_surface, &layer_shell->events.new_surface),

      // Cleanup listeners
      backend_destroy(this, ::backend_destroy, &backend->events.destroy),
      xdg_shell_destroy(this, ::xdg_shell_destroy, &xdg_shell->events.destroy),
      layer_shell_destroy(this, ::layer_shell_destroy, &layer_shell->events.destroy) {
    if(!backend)
        throw std::runtime_error("failed to create wlr_backend");

    if(!renderer)
        throw std::runtime_error("failed to create wlr_renderer");

    // Initializes wl_shm, dmabuf, etc.
    if(!wlr_renderer_init_wl_display(renderer, display))
        throw std::runtime_error("wlr_renderer_init_wl_display failed");

    if(!allocator)
        throw std::runtime_error("failed to create wlr_allocator");

    // wl_compositor global.
    // Needed for clients to create surfaces
    wlr_compositor_create(display, 6, renderer);

    // wl_subcompositor global.
    // Needed for clients to create subsurfaces
    wlr_subcompositor_create(display);

    // wl_data_device_manager global.
    // Needed for inter-client data transfer
    // (copy-and-paste, drag-and-drop, etc.)
    wlr_data_device_manager_create(display);

    wlr_xdg_output_manager_v1_create(display, root.output_layout);
}

Server::~Server() {
    wl_display_destroy_clients(display);

    wlr_scene_node_destroy(&root.scene->tree.node);

    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(display);
}

void Server::start(char* startup_cmd) {
    std::string socket;
    for(int cur = 1; cur <= 32; cur++) {
        socket = "wayland-" + std::to_string(cur);
        int ret = wl_display_add_socket(display, socket.c_str());
        if(!ret)
            break;
        else
            wlr_log(WLR_INFO, "wl_display_add_socket for %s returned %d: skipping", socket.c_str(),
                    ret);
    }

    if(!wlr_backend_start(backend))
        throw std::runtime_error("couldn't start backend");

    setenv("WAYLAND_DISPLAY", socket.c_str(), true);
    if(startup_cmd) {
        if(fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void*)nullptr);
    }

    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket.c_str());
    wl_display_run(display);
}

template <typename T>
T* Server::surface_at(double lx, double ly, wlr_surface*& surface, double& sx, double& sy) {
    wlr_scene_node* node = wlr_scene_node_at(&root.scene->tree.node, lx, ly, &sx, &sy);
    if(!node || node->type != WLR_SCENE_NODE_BUFFER)
        return nullptr;

    wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
    wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
    if(!scene_surface || !scene_surface->surface)
        return nullptr;

    surface = scene_surface->surface;

    wlr_scene_tree* tree = node->parent;
    while(tree && !tree->node.data) {
        tree = tree->node.parent;
    }

    if(!tree || !tree->node.parent)
        return nullptr;

    return static_cast<T*>(tree->node.data);
}

// Wrapper functions around surface_at
// Necessary to force the instatiation of all templates we need
xdg_shell::Toplevel* Server::toplevel_at(double lx, double ly, wlr_surface*& surface, double& sx,
                                         double& sy) {
    xdg_shell::Toplevel* toplevel = surface_at<xdg_shell::Toplevel>(lx, ly, surface, sx, sy);
    if(surface && surface->mapped && strcmp(surface->role->name, "zwlr_layer_surface_v1") != 0)
        return toplevel;
    else
        return nullptr;
}

layer_shell::LayerSurface* Server::layer_surface_at(double lx, double ly, wlr_surface*& surface,
                                                    double& sx, double& sy) {
    layer_shell::LayerSurface* layer_surface =
        surface_at<layer_shell::LayerSurface>(lx, ly, surface, sx, sy);
    if(surface && surface->mapped && strcmp(surface->role->name, "zwlr_layer_surface_v1") == 0)
        return layer_surface;
    else
        return nullptr;
}

void backend_destroy(wl_listener* listener, void* data) {
    server.new_output.free();
    server.backend_destroy.free();
}

void xdg_shell_destroy(wl_listener* listener, void* data) {
    server.new_xdg_toplevel.free();
    server.xdg_shell_destroy.free();
}

void layer_shell_destroy(wl_listener* listener, void* data) {
    server.new_layer_shell_surface.free();
    server.layer_shell_destroy.free();
}
