#include "layer-shell.hpp"

#include <cassert>

#include "server.hpp"

namespace layer_shell {
    void new_surface(wl_listener *listener, void *data) {
        wlr_layer_surface_v1 *layer_surface = static_cast<wlr_layer_surface_v1 *>(data);

        if(!layer_surface->output) {
            if(server.root.outputs.empty()) {
                wlr_log(WLR_ERROR, "no output to assign layer surface '%s' to",
                        layer_surface->namespace_);
                wlr_layer_surface_v1_destroy(layer_surface);
                return;
            }
            layer_surface->output = server.root.outputs.front()->output;
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

    void map(wl_listener *listener, void *data) {
        LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
        wlr_scene_node_set_enabled(&surface->scene->tree->node, true);

        if(surface->layer_surface->current.keyboard_interactive !=
           ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
            wl_signal_emit(&server.root.events.new_node, static_cast<void *>(&surface->node));

        surface->output->arrange_layers();
    }

    void unmap(wl_listener *listener, void *data) {
        LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
        wlr_scene_node_set_enabled(&surface->scene->tree->node, false);

        wl_signal_emit(&surface->node.events.node_destroy, static_cast<void *>(&surface->node));
    }

    void surface_commit(wl_listener *listener, void *data) {
        LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
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

    void output_destroy(wl_listener *listener, void *data) {
        LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
        wlr_layer_surface_v1_destroy(surface->layer_surface);
    }

    void destroy(wl_listener *listener, void *data) {
        delete static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
    }

    void new_popup(wl_listener *listener, void *data) {
        LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
        wlr_xdg_popup *xdg_popup = static_cast<wlr_xdg_popup *>(data);

        surface->popups.push_back(new xdg_shell::Popup(xdg_popup, surface->scene->tree));
    }

    LayerSurface::LayerSurface(wlr_scene_layer_surface_v1 *scene, output::Output *output)
        : layer_surface(scene->layer_surface),
          node(this),
          scene(scene),
          output(output),
          popup_tree(wlr_scene_tree_create(server.root.layer_popups)),
          tree(scene->tree),

          map(this, layer_shell::map, &layer_surface->surface->events.map),
          unmap(this, layer_shell::unmap, &layer_surface->surface->events.unmap),
          surface_commit(this, layer_shell::surface_commit, &layer_surface->surface->events.commit),
          output_destroy(this, layer_shell::output_destroy, &output->output->events.destroy),
          node_destroy(this, layer_shell::destroy, &layer_surface->events.destroy),
          new_popup(this, layer_shell::new_popup, &layer_surface->events.new_popup) {
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
}
