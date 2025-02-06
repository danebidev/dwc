#include "server.h"

#include <unistd.h>

#include <cassert>
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
    new_output.notify = output::new_output;
    wl_signal_add(&backend->events.new_output, &new_output);

    // Creates the scene graph, that handles all rendering and damage tracking
    scene = wlr_scene_create();

    // Attaches an output layout to a scene, to synchronize the positions of scene
    // outputs with the positions of corresponding layout outputs
    scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

    // xdg-shell
    xdg_shell = wlr_xdg_shell_create(display, 6);

    new_xdg_toplevel.notify = toplevel::new_xdg_toplevel;
    new_xdg_popup.notify = toplevel::new_xdg_popup;
    wl_signal_add(&xdg_shell->events.new_toplevel, &new_xdg_toplevel);
    wl_signal_add(&xdg_shell->events.new_popup, &new_xdg_popup);

    // cursor is a wlroots utility for tracking the cursor image
    cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(cursor, output_layout);

    // Creates a xcursor manager, that loads Xcursor cursors
    // and manages scaling them
    cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    // Cursor input
    cursor_mode = cursor::CursorMode::PASSTHROUGH;

    cursor_motion.notify = cursor::cursor_motion;
    cursor_motion_absolute.notify = cursor::cursor_motion_absolute;
    cursor_button.notify = cursor::cursor_button;
    cursor_axis.notify = cursor::cursor_axis;
    cursor_frame.notify = cursor::cursor_frame;

    wl_signal_add(&cursor->events.motion, &cursor_motion);
    wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);
    wl_signal_add(&cursor->events.button, &cursor_button);
    wl_signal_add(&cursor->events.axis, &cursor_axis);
    wl_signal_add(&cursor->events.frame, &cursor_frame);

    // Seat
    seat = wlr_seat_create(display, "seat0");
    new_input.notify = seat::new_input;
    request_cursor.notify = seat::request_cursor;
    request_set_selection.notify = seat::request_set_selection;

    wl_signal_add(&backend->events.new_input, &new_input);
    wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
    wl_signal_add(&seat->events.request_set_selection, &request_set_selection);
}

Server::~Server() {
    wl_display_destroy_clients(display);

    wl_list_remove(&new_output.link);
    wl_list_remove(&new_xdg_toplevel.link);
    wl_list_remove(&new_xdg_popup.link);
    wl_list_remove(&new_input.link);

    wl_list_remove(&cursor_motion.link);
    wl_list_remove(&cursor_motion_absolute.link);
    wl_list_remove(&cursor_button.link);
    wl_list_remove(&cursor_axis.link);
    wl_list_remove(&cursor_frame.link);

    wl_list_remove(&request_cursor.link);
    wl_list_remove(&request_set_selection.link);

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
    toplevel::Toplevel* toplevel = server.grabbed_toplevel;
    wlr_scene_node_set_position(&toplevel->scene_tree->node, server.cursor->x - server.grab_x,
                                server.cursor->y - server.grab_y);
}

void Server::process_cursor_resize() {
    Server& server = Server::instance();
    toplevel::Toplevel* toplevel = server.grabbed_toplevel;
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
            execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void*)NULL);
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
    wlr_surface* surface = NULL;
    toplevel::Toplevel* toplevel =
        toplevel::desktop_toplevel_at(cursor->x, cursor->y, &sx, &sy, &surface);

    // If there's no toplevel to provide the image, set it to a default image
    if(!toplevel)
        wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
    if(surface) {
        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(seat, time, sx, sy);
    }
    else
        wlr_seat_pointer_clear_focus(seat);
}

void Server::begin_interactive(toplevel::Toplevel* toplevel, cursor::CursorMode mode,
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
