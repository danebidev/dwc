#include "layer-shell.hpp"

#include "server.hpp"

layer_shell::LayerSurface::LayerSurface(wlr_scene_layer_surface_v1 *scene, output::Output *output)
    : layer_surface(scene->layer_surface),
      scene(scene),
      popup_tree(wlr_scene_tree_create(Server::instance().layers.popups)),
      tree(scene->tree),
      output(output),
      map(this, layer_shell::map, &layer_surface->surface->events.map),
      unmap(this, layer_shell::unmap, &layer_surface->surface->events.unmap),
      surface_commit(this, layer_shell::surface_commit, &layer_surface->surface->events.commit),
      output_destroy(this, layer_shell::output_destroy, &output->output->events.destroy),
      node_destroy(this, layer_shell::destroy, &layer_surface->events.destroy),
      new_popup(this, layer_shell::surface_commit, &layer_surface->events.new_popup) {
    layer_surface->data = this;
    tree->node.data = this;
}

bool layer_shell::LayerSurface::should_focus() {
    if(!layer_surface || !layer_surface->surface || !layer_surface->surface->mapped)
        return false;

    return layer_surface->current.keyboard_interactive !=
           ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
}

void layer_shell::LayerSurface::handle_focus() {
    if(!should_focus())
        return;

    Server &server = Server::instance();

    wlr_keyboard *keyboard = wlr_seat_get_keyboard(server.seat.seat);

    if(keyboard)
        wlr_seat_keyboard_enter(server.seat.seat, layer_surface->surface, keyboard->keycodes,
                                keyboard->num_keycodes, &keyboard->modifiers);
}

void layer_shell::new_surface(wl_listener *listener, void *data) {
    wlr_layer_surface_v1 *layer_surface = static_cast<wlr_layer_surface_v1 *>(data);
    Server &server = Server::instance();

    if(!layer_surface->output) {
        if(server.outputs.empty()) {
            wlr_log(WLR_ERROR, "no output to assign layer surface '%s' to",
                    layer_surface->namespace_);
        }
        layer_surface->output = server.outputs[0]->output;
    }

    output::Output *output = static_cast<output::Output *>(layer_surface->output->data);
    // Get requested layer type
    zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;

    wlr_scene_tree *output_layer = get_scene(output, layer_type);
    wlr_scene_layer_surface_v1 *scene_surface =
        wlr_scene_layer_surface_v1_create(output_layer, layer_surface);

    new LayerSurface(scene_surface, output);
}

wlr_scene_tree *layer_shell::get_scene(output::Output *output, zwlr_layer_shell_v1_layer type) {
    switch(type) {
        case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
            return output->layers.shell_background;
        case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
            return output->layers.shell_bottom;
        case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
            return output->layers.shell_top;
        case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
            return output->layers.shell_overlay;
    }
}

void layer_shell::map(wl_listener *listener, void *data) {
    LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
    wlr_layer_surface_v1 *layer_surface = surface->layer_surface;
    wlr_scene_node_set_enabled(&surface->scene->tree->node, true);

    if(layer_surface->current.keyboard_interactive &&
       (layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
        layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
        surface->handle_focus();
    }

    surface->output->arrange();
}

void layer_shell::unmap(wl_listener *listener, void *data) {
    LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
    wlr_scene_node_set_enabled(&surface->scene->tree->node, false);
}

void layer_shell::surface_commit(wl_listener *listener, void *data) {
    LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;

    bool arrange = false;

    if(surface->layer_surface->current.committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
        wlr_scene_tree *new_tree =
            get_scene(surface->output, surface->layer_surface->current.layer);
        wlr_scene_node_reparent(&surface->scene->tree->node, new_tree);
        arrange = true;
    }

    if(!surface->layer_surface->configured)
        wlr_layer_surface_v1_configure(surface->layer_surface, 0, 0);

    if(arrange)
        surface->output->arrange();
}

void layer_shell::output_destroy(wl_listener *listener, void *data) {
    LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
    wlr_layer_surface_v1_destroy(surface->layer_surface);
}

void layer_shell::destroy(wl_listener *listener, void *data) {
    delete static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
}

void layer_shell::new_popup(wl_listener *listener, void *data) {
    LayerSurface *surface = static_cast<wrapper::Listener<LayerSurface> *>(listener)->container;
    wlr_xdg_popup *xdg_popup = static_cast<wlr_xdg_popup *>(data);

    surface->popups.push_back(new xdg_shell::Popup(xdg_popup, surface->tree));
}
