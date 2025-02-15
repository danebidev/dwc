#include "xdg-shell.hpp"

#include <algorithm>
#include <cassert>

#include "server.hpp"

namespace xdg_shell {
    void new_xdg_toplevel(wl_listener* listener, void* data) {
        wlr_xdg_toplevel* xdg_toplevel = static_cast<wlr_xdg_toplevel*>(data);

        new Toplevel(xdg_toplevel);
    }

    // Called when a surface gets mapped
    void xdg_toplevel_map(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        server.toplevels.push_back(toplevel);
        wl_signal_emit(&server.root.events.new_node, static_cast<void*>(&toplevel->node));
    }

    // Called when an xdg_toplevel gets unmapped
    void xdg_toplevel_unmap(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;

        // Reset cursor mode if the toplevel was currently focused
        if(toplevel == server.input_manager.seat.cursor.grabbed_toplevel)
            server.input_manager.seat.cursor.reset_cursor_mode();

        wl_signal_emit(&toplevel->node.events.node_destroy, static_cast<void*>(&toplevel->node));
        server.toplevels.remove(toplevel);
    }

    // Called when a commit gets applied to a toplevel
    void xdg_toplevel_commit(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;

        if(toplevel->toplevel->base->initial_commit) {
            // Set size to 0,0 so the client can choose the size
            wlr_xdg_toplevel_set_size(toplevel->toplevel, 0, 0);
        }
    }

    // Called when an xdg_toplevel gets destroyed
    void xdg_toplevel_destroy(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        delete toplevel;
    }

    // Called when an xdg_toplevel requests a move
    void xdg_toplevel_request_move(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        server.input_manager.seat.cursor.begin_interactive(toplevel, cursor::CursorMode::MOVE, 0);
    }

    // Called when an xdg_toplevel requests a resize
    void xdg_toplevel_request_resize(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        wlr_xdg_toplevel_resize_event* event = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        server.input_manager.seat.cursor.begin_interactive(toplevel, cursor::CursorMode::RESIZE,
                                                           event->edges);
    }

    // Called when an xdg_toplevel requests to be maximized
    void xdg_toplevel_request_maximize(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        if(toplevel->toplevel->base->initialized)
            wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
    }

    // Called when an xdg_toplevel requests fullscreen
    void xdg_toplevel_request_fullscreen(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        if(toplevel->toplevel->base->initialized)
            wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
    }

    // Called when a popup is created by a client
    void xdg_toplevel_new_popup(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);

        new Popup(xdg_popup, toplevel->scene_tree);
    }

    void xdg_popup_commit(wl_listener* listener, void* data) {
        Popup* popup = static_cast<wrapper::Listener<Popup>*>(listener)->container;

        if(popup->popup->base->initial_commit)
            wlr_xdg_surface_schedule_configure(popup->popup->base);
    }

    void xdg_popup_destroy(wl_listener* listener, void* data) {
        delete static_cast<wrapper::Listener<Popup>*>(listener)->container;
    }

    void xdg_popup_new_popup(wl_listener* listener, void* data) {
        Popup* popup = static_cast<wrapper::Listener<Popup>*>(listener)->container;
        wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);

        new Popup(xdg_popup, popup->scene);
    }

    Toplevel::Toplevel(wlr_xdg_toplevel* xdg_toplevel)
        : toplevel(xdg_toplevel),
          node(this),
          scene_tree(wlr_scene_xdg_surface_create(server.root.floating, toplevel->base)),

          map(this, xdg_toplevel_map, &toplevel->base->surface->events.map),
          unmap(this, xdg_toplevel_unmap, &toplevel->base->surface->events.unmap),
          commit(this, xdg_toplevel_commit, &toplevel->base->surface->events.commit),
          destroy(this, xdg_toplevel_destroy, &toplevel->events.destroy),

          request_move(this, xdg_toplevel_request_move, &toplevel->events.request_move),
          request_resize(this, xdg_toplevel_request_resize, &toplevel->events.request_resize),
          request_maximize(this, xdg_toplevel_request_maximize, &toplevel->events.request_maximize),
          request_fullscreen(this, xdg_toplevel_request_fullscreen,
                             &toplevel->events.request_fullscreen),

          new_popup(this, xdg_toplevel_new_popup, &toplevel->base->events.new_popup) {
        toplevel->base->data = scene_tree;
        scene_tree->node.data = this;
    }

    Popup::Popup(wlr_xdg_popup* xdg_popup, wlr_scene_tree* parent_tree)
        : popup(xdg_popup),

          scene(wlr_scene_xdg_surface_create(parent_tree, popup->base)),

          commit(this, xdg_popup_commit, &popup->base->surface->events.commit),
          destroy(this, xdg_popup_destroy, &popup->events.destroy),
          new_popup(this, xdg_popup_new_popup, &popup->base->events.new_popup) {
        xdg_popup->base->data = scene;
    }
}
