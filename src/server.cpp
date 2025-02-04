#include "server.h"

#include <stdexcept>

#include "output.h"
#include "wlr.h"

Server::Server() {
    // wl_display global.
    // Needed for the registry and the creation of more objects
    display = wl_display_create();

    // Backend. Abstracts input and output hardware.
    // Supports stuff like X11 windows, DRM, libinput, headless, etc.
    backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL);
    if(!backend)
        throw std::runtime_error("failed to create wlr_backend");

    // Automatically chooses a wlr renderer. Can be Pixman, GLES2 or Vulkan.
    // Can be overridden by the user with the WLR_RENDERER env var
    renderer = wlr_renderer_autocreate(backend);
    if(!renderer)
        throw std::runtime_error("failed to create wlr_renderer");

    // Initializes wl_shm, dmabuf, etc.
    if(!wlr_renderer_init_wl_display(renderer, display))
        throw std::runtime_error("wlr_renderer_init_wl_display failed");

    // Creates an allocator. The allocator allocates pixel buffers with
    // the correct capabilities and position based on the backend and renderer
    allocator = wlr_allocator_autocreate(backend, renderer);
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

    // Output layout, to configure how
    // outputs are physically arranged
    output_layout = wlr_output_layout_create(display);

    // Listener for new available outputs
    new_output_listener.notify = output::new_output;
    wl_signal_add(&backend->events.new_output, &new_output_listener);

    // Creates the scene graph, that handles all rendering and damage tracking
    scene = wlr_scene_create();

    // Attaches an output layout to a scene, to synchronize the positions of scene
    // outputs with the positions of corresponding layout outputs
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);
}

Server::~Server() {
    wl_display_destroy_clients(display);

    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(display);
}

Server& Server::instance() {
    static Server instance;
    return instance;
}

void Server::start() {
    const char* socket = wl_display_add_socket_auto(display);
    if(!socket) {
        wlr_backend_destroy(backend);
        throw std::runtime_error("couldn't create socket");
    }

    wlr_backend_start(backend);

    wlr_log(WLR_INFO, "Running compositor");
    wl_display_run(display);
}
