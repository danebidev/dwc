#include "layer-shell.hpp"

#include <cassert>

#include "server.hpp"

namespace layer_shell {
    void new_surface(Server *server, void *data) {
        wlr_layer_surface_v1 *layer_surface = static_cast<wlr_layer_surface_v1 *>(data);

        if(!layer_surface->output) {
            if(server->output_manager.outputs.empty()) {
                logger.log(LogLevel::ERROR, "No output to assign layer surface '{}' to",
                           layer_surface->namespace_);
                wlr_layer_surface_v1_destroy(layer_surface);
                return;
            }
            output::Output *output = server->output_manager.focused_output();
            if(output)
                layer_surface->output = output->output;
            else
                layer_surface->output = server->output_manager.outputs.front()->output;
        }

        assert(layer_surface->output);
        output::Output *output = static_cast<output::Output *>(layer_surface->output->data);
        // Get requested layer type
        zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;

        wlr_scene_tree *output_layer = output->get_scene(layer_type);
        wlr_scene_layer_surface_v1 *scene_surface =
            wlr_scene_layer_surface_v1_create(output_layer, layer_surface);

        new LayerSurface(scene_surface, output);
    }

    LayerSurface::LayerSurface(wlr_scene_layer_surface_v1 *scene, output::Output *output)
        : layer_surface(scene->layer_surface),
          node(this),
          scene(scene),
          output(output),
          popup_tree(wlr_scene_tree_create(server.root.layer_popups)),
          tree(scene->tree),

          map_list(LISTEN(layer_surface->surface->events.map, LayerSurface::map)),
          unmap_list(LISTEN(layer_surface->surface->events.unmap, LayerSurface::unmap)),
          commit_list(LISTEN(layer_surface->surface->events.commit, LayerSurface::commit)),
          output_destroy_list(LISTEN(output->output->events.destroy, LayerSurface::output_destroy)),
          destroy_list(LISTEN(layer_surface->events.destroy, LayerSurface::destroy)),
          new_popup_list(LISTEN(layer_surface->events.new_popup, LayerSurface::new_popup)) {
        layer_surface->data = this;
        tree->node.data = this;
    }

    void LayerSurface::handle_focus() {
        if(!layer_surface || !layer_surface->surface || !layer_surface->surface->mapped)
            return;

        if(layer_surface->current.keyboard_interactive ==
           ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
            return;

        server.input_manager.seat.focus_node(&node);
    }

    void LayerSurface::map(LayerSurface *surface, void *data) {
        wlr_scene_node_set_enabled(&surface->scene->tree->node, true);

        if(surface->layer_surface->current.keyboard_interactive !=
           ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
            wl_signal_emit(&server.root.events.new_node, static_cast<void *>(&surface->node));

        surface->output->arrange_layers();
    }

    void LayerSurface::unmap(LayerSurface *surface, void *data) {
        wlr_scene_node_set_enabled(&surface->scene->tree->node, false);

        wl_signal_emit(&surface->node.events.node_destroy, static_cast<void *>(&surface->node));
    }

    void LayerSurface::commit(LayerSurface *surface, void *data) {
        bool rearrange = false;
        // HACK
        // TODO: actually figure out when to call this
        server.root.arrange();

        if(surface->layer_surface->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
            wlr_scene_tree *new_tree =
                surface->output->get_scene(surface->layer_surface->current.layer);
            wlr_scene_node_reparent(&surface->scene->tree->node, new_tree);
            rearrange = true;
        }

        if(surface->layer_surface->initial_commit || rearrange) {
            wlr_layer_surface_v1_configure(surface->layer_surface, 0, 0);
            surface->output->arrange_layers();
        }
    }

    void LayerSurface::output_destroy(LayerSurface *surface, void *data) {
        wlr_layer_surface_v1_destroy(surface->layer_surface);
    }

    void LayerSurface::destroy(LayerSurface *surface, void *data) {
        delete surface;
    }

    void LayerSurface::new_popup(LayerSurface *surface, void *data) {
        wlr_xdg_popup *xdg_popup = static_cast<wlr_xdg_popup *>(data);

        surface->popups.push_back(new xdg_shell::Popup(xdg_popup, surface->scene->tree));
    }
}
