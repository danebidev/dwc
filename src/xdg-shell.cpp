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

        output::Output* output = server.output_manager.focused_output();
        if(!output && !server.output_manager.outputs.empty())
            output = server.output_manager.outputs.front();

        if(output) {
            assert(output->active_workspace);

            toplevel->workspace = output->active_workspace;
            toplevel->workspace->floating.push_back(toplevel);

            wlr_surface_set_preferred_buffer_scale(toplevel->toplevel->base->surface,
                                                   output->output->scale);

            wlr_box* usable_area = &output->usable_area;

            uint32_t width = toplevel->toplevel->scheduled.width > 0
                                 ? toplevel->toplevel->scheduled.width
                                 : toplevel->toplevel->current.width;

            uint32_t height = toplevel->toplevel->scheduled.height > 0
                                  ? toplevel->toplevel->scheduled.height
                                  : toplevel->toplevel->current.height;

            if(!width || !height) {
                width = toplevel->toplevel->base->surface->current.width;
                height = toplevel->toplevel->base->surface->current.height;
            }

            width = std::min(width, (uint32_t)usable_area->width);
            height = std::min(height, (uint32_t)usable_area->height);

            int x = output->output_box.x + (usable_area->width - width) / 2;
            int y = output->output_box.y + (usable_area->height - height) / 2;

            x = std::max(x, usable_area->x);
            y = std::max(y, usable_area->y);

            wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);
        }

        server.toplevels.push_back(toplevel);
        wl_signal_emit(&server.root.events.new_node, static_cast<void*>(&toplevel->node));
    }

    // Called when an xdg_toplevel gets unmapped
    void xdg_toplevel_unmap(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;

        // Reset cursor mode if the toplevel was currently grabbed
        if(toplevel == server.input_manager.seat.cursor.grabbed_toplevel)
            server.input_manager.seat.cursor.reset_cursor_mode();

        for(auto& ws : server.root.workspaces) {
            if(ws.second->focused_toplevel == toplevel)
                ws.second->focused_toplevel = nullptr;
        }

        wl_signal_emit(&toplevel->node.events.node_destroy, static_cast<void*>(&toplevel->node));
        server.toplevels.remove(toplevel);
        toplevel->workspace->floating.remove(toplevel);
        if(toplevel->workspace->focused_toplevel == toplevel)
            toplevel->workspace->focused_toplevel = nullptr;
    }

    // Called when a commit gets applied to a toplevel
    void xdg_toplevel_commit(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;

        if(toplevel->toplevel->base->initial_commit)
            // Set size to 0,0 so the client can choose the size
            wlr_xdg_toplevel_set_size(toplevel->toplevel, 0, 0);
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
        toplevel->fullscreen();
    }

    // Called when an xdg_toplevel requests to be minimized
    void xdg_toplevel_request_minimize(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        if(toplevel->toplevel->base->initialized)
            wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
    }

    // Called when an xdg_toplevel requests fullscreen
    void xdg_toplevel_request_fullscreen(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        toplevel->fullscreen();
    }

    // Called when a popup is created by a client
    void xdg_toplevel_new_popup(wl_listener* listener, void* data) {
        Toplevel* toplevel = static_cast<wrapper::Listener<Toplevel>*>(listener)->container;
        wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);

        new Popup(xdg_popup, toplevel->scene_tree);
    }

    void xdg_popup_commit(wl_listener* listener, void* data) {
        Popup* popup = static_cast<wrapper::Listener<Popup>*>(listener)->container;
        output::Output* output = server.output_manager.focused_output();

        if(!output)
            return;

        int lx, ly;
        wlr_scene_node_coords(&popup->scene->node.parent->node, &lx, &ly);

        // the output box expressed in the coordinate system of the parent
        // of the popup
        struct wlr_box box = {
            .x = output->output_box.x - lx,
            .y = output->output_box.y - ly,
            .width = output->output_box.width,
            .height = output->output_box.height,
        };

        wlr_xdg_popup_unconstrain_from_box(popup->popup, &box);

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
          workspace(nullptr),

          map(this, xdg_toplevel_map, &toplevel->base->surface->events.map),
          unmap(this, xdg_toplevel_unmap, &toplevel->base->surface->events.unmap),
          commit(this, xdg_toplevel_commit, &toplevel->base->surface->events.commit),
          destroy(this, xdg_toplevel_destroy, &toplevel->events.destroy),

          request_move(this, xdg_toplevel_request_move, &toplevel->events.request_move),
          request_resize(this, xdg_toplevel_request_resize, &toplevel->events.request_resize),
          request_maximize(this, xdg_toplevel_request_maximize, &toplevel->events.request_maximize),
          request_minimize(this, xdg_toplevel_request_minimize, &toplevel->events.request_minimize),
          request_fullscreen(this, xdg_toplevel_request_fullscreen,
                             &toplevel->events.request_fullscreen),

          new_popup(this, xdg_toplevel_new_popup, &toplevel->base->events.new_popup) {
        toplevel->base->data = scene_tree;
        scene_tree->node.data = this;
    }

    output::Output* Toplevel::output() {
        return server.output_manager.output_at(scene_tree->node.x, scene_tree->node.y);
    }

    void Toplevel::fullscreen() {
        // Unfullscreen
        if(workspace->focused_toplevel == this && workspace->fullscreen) {
            wlr_xdg_toplevel_set_size(toplevel, saved_geometry.width, saved_geometry.height);
            wlr_scene_node_reparent(&scene_tree->node, server.root.floating);
            wlr_xdg_toplevel_set_fullscreen(toplevel, false);

            wlr_scene_node_set_position(&scene_tree->node, saved_geometry.x, saved_geometry.y);

            workspace->focused_toplevel = this;
            workspace->fullscreen = false;
        }
        // Fullscreen
        else {
            // Unfullscreen the previous fullscreened toplevel
            if(workspace->focused_toplevel && workspace->fullscreen)
                workspace->focused_toplevel->fullscreen();

            saved_geometry = toplevel->base->geometry;
            saved_geometry.x = scene_tree->node.x;
            saved_geometry.y = scene_tree->node.y;

            output::Output* out = workspace->output;
            wlr_scene_node_set_position(&scene_tree->node, out->output_box.x, out->output_box.y);
            wlr_xdg_toplevel_set_size(toplevel, out->output_box.width, out->output_box.height);

            wlr_scene_node_raise_to_top(&scene_tree->node);
            wlr_scene_node_reparent(&scene_tree->node, server.root.fullscreen);
            wlr_xdg_toplevel_set_fullscreen(toplevel, true);

            workspace->focused_toplevel = this;
            workspace->fullscreen = true;
        }

        wlr_xdg_surface_schedule_configure(toplevel->base);
        workspace->focus();
        server.root.arrange();
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
