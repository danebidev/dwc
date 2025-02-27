#include "xdg-shell.hpp"

#include <algorithm>
#include <cassert>

#include "server.hpp"

namespace xdg_shell {
    void new_xdg_toplevel(Server* listener, void* data) {
        wlr_xdg_toplevel* xdg_toplevel = static_cast<wlr_xdg_toplevel*>(data);

        new Toplevel(xdg_toplevel);
    }

    void Toplevel::map(Toplevel* toplevel, void* data) {
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
    void Toplevel::unmap(Toplevel* toplevel, void* data) {
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

    void Toplevel::commit(Toplevel* toplevel, void* data) {
        if(toplevel->toplevel->base->initial_commit)
            // Set size to 0,0 so the client can choose the size
            wlr_xdg_toplevel_set_size(toplevel->toplevel, 0, 0);
    }

    void Toplevel::destroy(Toplevel* toplevel, void* data) {
        delete toplevel;
    }

    void Toplevel::request_move(Toplevel* toplevel, void* data) {
        server.input_manager.seat.cursor.begin_interactive(toplevel, cursor::CursorMode::MOVE, 0);
    }

    void Toplevel::request_resize(Toplevel* toplevel, void* data) {
        wlr_xdg_toplevel_resize_event* event = static_cast<wlr_xdg_toplevel_resize_event*>(data);
        server.input_manager.seat.cursor.begin_interactive(toplevel, cursor::CursorMode::RESIZE,
                                                           event->edges);
    }

    void Toplevel::request_minimize(Toplevel* toplevel, void* data) {
        if(toplevel->toplevel->base->initialized)
            wlr_xdg_surface_schedule_configure(toplevel->toplevel->base);
    }

    void Toplevel::request_fullscreen(Toplevel* toplevel, void* data) {
        toplevel->fullscreen();
    }

    void Toplevel::new_popup(Toplevel* toplevel, void* data) {
        wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);
        new Popup(xdg_popup, toplevel->scene_tree);
    }

    void Popup::commit(Popup* popup, void* data) {
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

    void Popup::destroy(Popup* popup, void* data) {
        delete popup;
    }

    void Popup::new_popup(Popup* popup, void* data) {
        wlr_xdg_popup* xdg_popup = static_cast<wlr_xdg_popup*>(data);
        new Popup(xdg_popup, popup->scene);
    }

    Toplevel::Toplevel(wlr_xdg_toplevel* xdg_toplevel)
        : toplevel(xdg_toplevel),
          node(this),
          scene_tree(wlr_scene_xdg_surface_create(server.root.floating, toplevel->base)),
          workspace(nullptr),

          map_list(LISTEN(toplevel->base->surface->events.map, Toplevel::map)),
          unmap_list(LISTEN(toplevel->base->surface->events.unmap, Toplevel::unmap)),
          commit_list(LISTEN(toplevel->base->surface->events.commit, Toplevel::commit)),
          destroy_list(LISTEN(toplevel->events.destroy, Toplevel::destroy)),

          new_popup_list(LISTEN(toplevel->base->events.new_popup, Toplevel::new_popup)),

          request_move_list(LISTEN(toplevel->events.request_move, Toplevel::request_move)),
          request_resize_list(LISTEN(toplevel->events.request_resize, Toplevel::request_resize)),
          request_maximize_list(
              LISTEN(toplevel->events.request_maximize, Toplevel::request_fullscreen)),
          request_minimize_list(
              LISTEN(toplevel->events.request_minimize, Toplevel::request_minimize)),
          request_fullscreen_list(
              LISTEN(toplevel->events.request_fullscreen, Toplevel::request_fullscreen)) {
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

            update_fullscreen();
        }

        wlr_xdg_surface_schedule_configure(toplevel->base);
        workspace->focus();
        server.root.arrange();
    }

    void Toplevel::update_fullscreen() {
        output::Output* output = workspace->output;
        wlr_scene_node_set_position(&scene_tree->node, output->output_box.x, output->output_box.y);
        wlr_xdg_toplevel_set_size(toplevel, output->output_box.width, output->output_box.height);

        wlr_scene_node_raise_to_top(&scene_tree->node);
        wlr_scene_node_reparent(&scene_tree->node, server.root.fullscreen);
        wlr_xdg_toplevel_set_fullscreen(toplevel, true);

        workspace->focused_toplevel = this;
        workspace->fullscreen = true;
    }

    Popup::Popup(wlr_xdg_popup* xdg_popup, wlr_scene_tree* parent_tree)
        : popup(xdg_popup),

          scene(wlr_scene_xdg_surface_create(parent_tree, popup->base)),

          commit_list(LISTEN(popup->base->surface->events.commit, Popup::commit)),
          destroy_list(LISTEN(popup->events.destroy, Popup::destroy)),
          new_popup_list(LISTEN(popup->base->events.new_popup, Popup::new_popup)) {
        xdg_popup->base->data = scene;
    }
}
