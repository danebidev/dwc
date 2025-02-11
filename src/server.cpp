#include "server.hpp"

#include <unistd.h>

#include <cassert>
#include <stdexcept>

#include "layer-shell.hpp"
#include "output.hpp"
#include "wlr.hpp"

RootLayers::RootLayers(wlr_scene_tree* parent)
    : shell_background(wlr_scene_tree_create(parent)),
      shell_bottom(wlr_scene_tree_create(parent)),
      shell_top(wlr_scene_tree_create(parent)),
      shell_overlay(wlr_scene_tree_create(parent)),

      floating(wlr_scene_tree_create(parent)),
      fullscreen(wlr_scene_tree_create(parent)),
      popups(wlr_scene_tree_create(parent)),
      seat(wlr_scene_tree_create(parent)) {}

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

      // Output layout, to configure how
      // outputs are physically arranged
      output_layout(wlr_output_layout_create(display)),

      // Creates the scene graph, that handles all rendering and damage tracking
      scene(wlr_scene_create()),
      layers(&scene->tree),

      // Attaches an output layout to a scene, to synchronize the positions of scene
      // outputs with the positions of corresponding layout outputs
      scene_layout(wlr_scene_attach_output_layout(scene, output_layout)),

      // protocols
      xdg_shell(wlr_xdg_shell_create(display, 6)),
      layer_shell(wlr_layer_shell_v1_create(display, 5)),

      // cursor is a wlroots utility for tracking the cursor image
      cursor(wlr_cursor_create()),

      // Creates a xcursor manager, that loads Xcursor cursors
      // and manages scaling them
      cursor_mgr(wlr_xcursor_manager_create(nullptr, 24)),
      cursor_mode(cursor::CursorMode::PASSTHROUGH),

      // Seat
      seat("seat0", display, layers.seat),

      // Listeners
      new_output(this, output::new_output, &backend->events.new_output),

      new_xdg_toplevel(this, xdg_shell::new_xdg_toplevel, &xdg_shell->events.new_toplevel),
      new_xdg_popup(this, xdg_shell::new_xdg_popup, &xdg_shell->events.new_popup),

      new_layer_shell_surface(this, layer_shell::new_surface, &layer_shell->events.new_surface),

      cursor_motion(this, cursor::cursor_motion, &cursor->events.motion),
      cursor_motion_absolute(this, cursor::cursor_motion_absolute, &cursor->events.motion_absolute),
      cursor_button(this, cursor::cursor_button, &cursor->events.button),
      cursor_axis(this, cursor::cursor_axis, &cursor->events.axis),
      cursor_frame(this, cursor::cursor_frame, &cursor->events.frame),

      new_input(this, seat::new_input, &backend->events.new_input) {
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

    wlr_cursor_attach_output_layout(cursor, output_layout);
}

Server::~Server() {
    wl_display_destroy_clients(display);

    // We have to destroy listeners before we can destroy the other
    // objects, so this is necessary. Ideally there'd be a cleaner way
    cursor_motion.free();
    cursor_motion_absolute.free();
    cursor_button.free();
    cursor_axis.free();
    cursor_frame.free();

    new_input.free();
    new_output.free();

    new_xdg_toplevel.free();
    new_xdg_popup.free();

    new_layer_shell_surface.free();

    seat.free_listeners();

    wlr_xcursor_manager_destroy(cursor_mgr);
    wlr_cursor_destroy(cursor);
    wlr_scene_node_destroy(&scene->tree.node);

    wlr_allocator_destroy(allocator);
    wlr_renderer_destroy(renderer);
    wlr_backend_destroy(backend);
    wl_display_destroy(display);
}

void Server::process_cursor_move() {
    Server& server = Server::instance();
    xdg_shell::Toplevel* toplevel = server.grabbed_toplevel;
    wlr_scene_node_set_position(&toplevel->scene_tree->node, server.cursor->x - server.grab_x,
                                server.cursor->y - server.grab_y);
}

void Server::process_cursor_resize() {
    Server& server = Server::instance();
    xdg_shell::Toplevel* toplevel = server.grabbed_toplevel;
    double border_x = server.cursor->x - server.grab_x;
    double border_y = server.cursor->y - server.grab_y;
    int new_left = server.grab_geobox.x;
    int new_right = server.grab_geobox.x + server.grab_geobox.width;
    int new_top = server.grab_geobox.y;
    int new_bottom = server.grab_geobox.y + server.grab_geobox.height;

    if(server.resize_edges & WLR_EDGE_TOP) {
        new_top = border_y;
        if(new_top >= new_bottom) {
            new_top = new_bottom - 1;
        }
    }
    else if(server.resize_edges & WLR_EDGE_BOTTOM) {
        new_bottom = border_y;
        if(new_bottom <= new_top) {
            new_bottom = new_top + 1;
        }
    }
    if(server.resize_edges & WLR_EDGE_LEFT) {
        new_left = border_x;
        if(new_left >= new_right) {
            new_left = new_right - 1;
        }
    }
    else if(server.resize_edges & WLR_EDGE_RIGHT) {
        new_right = border_x;
        if(new_right <= new_left) {
            new_right = new_left + 1;
        }
    }

    struct wlr_box* geo_box = &toplevel->toplevel->base->geometry;
    wlr_scene_node_set_position(&toplevel->scene_tree->node, new_left - geo_box->x,
                                new_top - geo_box->y);

    int new_width = new_right - new_left;
    int new_height = new_bottom - new_top;
    wlr_xdg_toplevel_set_size(toplevel->toplevel, new_width, new_height);
}

Server& Server::instance() {
    static Server instance;
    return instance;
}

void Server::start(char* startup_cmd) {
    const char* socket = wl_display_add_socket_auto(display);
    if(!socket) {
        wlr_backend_destroy(backend);
        throw std::runtime_error("couldn't create socket");
    }

    if(!wlr_backend_start(backend))
        throw std::runtime_error("couldn't start backend");

    setenv("WAYLAND_DISPLAY", socket, true);
    if(startup_cmd) {
        if(fork() == 0)
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void*)nullptr);
    }

    wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(display);
}

void Server::process_cursor_motion(uint32_t time) {
    if(cursor_mode == cursor::CursorMode::MOVE) {
        process_cursor_move();
        return;
    }
    else if(cursor_mode == cursor::CursorMode::RESIZE) {
        process_cursor_resize();
        return;
    }

    double sx, sy;
    wlr_surface* surface = nullptr;

    xdg_shell::Toplevel* toplevel = toplevel_at(cursor->x, cursor->y, surface, sx, sy);
    if(toplevel && surface && surface->mapped) {
        wlr_seat_pointer_notify_enter(seat.seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(seat.seat, time, sx, sy);
        return;
    }

    layer_shell::LayerSurface* layer_surface =
        layer_surface_at(cursor->x, cursor->y, surface, sx, sy);
    if(layer_surface && surface && surface->mapped) {
        wlr_seat_pointer_notify_enter(seat.seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(seat.seat, time, sx, sy);
        return;
    }

    wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    wlr_seat_pointer_clear_focus(seat.seat);
}

void Server::begin_interactive(xdg_shell::Toplevel* toplevel, cursor::CursorMode mode,
                               uint32_t edges) {
    assert(mode != cursor::CursorMode::PASSTHROUGH);
    Server& server = Server::instance();

    server.grabbed_toplevel = toplevel;
    server.cursor_mode = mode;

    if(mode == cursor::CursorMode::MOVE) {
        server.grab_x = server.cursor->x - toplevel->scene_tree->node.x;
        server.grab_y = server.cursor->y - toplevel->scene_tree->node.y;
    }
    else {
        wlr_box* geo_box = &toplevel->toplevel->base->geometry;
        double border_x = (toplevel->scene_tree->node.x + geo_box->x) +
                          ((edges & WLR_EDGE_RIGHT) ? geo_box->width : 0);
        double border_y = (toplevel->scene_tree->node.y + geo_box->y) +
                          ((edges & WLR_EDGE_BOTTOM) ? geo_box->height : 0);
        server.grab_x = server.cursor->x - border_x;
        server.grab_y = server.cursor->y - border_y;

        server.grab_geobox = *geo_box;
        server.grab_geobox.x += toplevel->scene_tree->node.x;
        server.grab_geobox.y += toplevel->scene_tree->node.y;

        server.resize_edges = edges;
    }
}

template <typename T>
T* Server::surface_at(double lx, double ly, wlr_surface*& surface, double& sx, double& sy) {
    wlr_scene_node* node = wlr_scene_node_at(&scene->tree.node, lx, ly, &sx, &sy);
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
