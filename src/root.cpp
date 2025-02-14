#include "root.hpp"

#include "server.hpp"

nodes::Node::Node(xdg_shell::Toplevel* toplevel)
    : type(NodeType::TOPLEVEL) {
    val.toplevel = toplevel;
    wl_signal_init(&events.node_destroy);
}

nodes::Node::Node(layer_shell::LayerSurface* layer_surface)
    : type(NodeType::LAYER_SURFACE) {
    val.layer_surface = layer_surface;
    wl_signal_init(&events.node_destroy);
}

bool nodes::Node::has_exclusivity() {
    return type == nodes::NodeType::LAYER_SURFACE &&
           val.layer_surface->layer_surface->current.keyboard_interactive ==
               ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE &&
           (val.layer_surface->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP ||
            val.layer_surface->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
}

nodes::Root::Root(wl_display* display)
    : scene(wlr_scene_create()),

      shell_background(wlr_scene_tree_create(&scene->tree)),
      shell_bottom(wlr_scene_tree_create(&scene->tree)),
      floating(wlr_scene_tree_create(&scene->tree)),
      toplevel_popups(wlr_scene_tree_create(&scene->tree)),
      shell_top(wlr_scene_tree_create(&scene->tree)),
      shell_overlay(wlr_scene_tree_create(&scene->tree)),
      layer_popups(wlr_scene_tree_create(&scene->tree)),
      seat(wlr_scene_tree_create(&scene->tree)),

      output_layout(wlr_output_layout_create(display)) {
    wl_signal_init(&events.new_node);
}

void nodes::Root::arrange() {
    wlr_scene_node_set_enabled(&shell_background->node, true);
    wlr_scene_node_set_enabled(&shell_bottom->node, true);
    wlr_scene_node_set_enabled(&floating->node, true);
    wlr_scene_node_set_enabled(&shell_top->node, true);
    wlr_scene_node_set_enabled(&shell_overlay->node, true);

    for(auto& output : server.outputs) {
        wlr_scene_output_set_position(output->scene_output, output->lx, output->ly);

        wlr_scene_node_reparent(&output->layers.shell_background->node, shell_background);
        wlr_scene_node_reparent(&output->layers.shell_bottom->node, shell_bottom);
        wlr_scene_node_reparent(&output->layers.shell_top->node, shell_top);
        wlr_scene_node_reparent(&output->layers.shell_overlay->node, shell_overlay);

        wlr_scene_node_set_position(&output->layers.shell_background->node, output->lx, output->ly);
        wlr_scene_node_set_position(&output->layers.shell_bottom->node, output->lx, output->ly);
        wlr_scene_node_set_position(&output->layers.shell_top->node, output->lx, output->ly);
        wlr_scene_node_set_position(&output->layers.shell_overlay->node, output->lx, output->ly);
    }
}
