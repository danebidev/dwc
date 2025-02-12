#include "root.hpp"

#include "server.hpp"

nodes::Root::Root(wlr_scene_tree* parent)
    : shell_background(wlr_scene_tree_create(parent)),
      shell_bottom(wlr_scene_tree_create(parent)),
      floating(wlr_scene_tree_create(parent)),
      shell_top(wlr_scene_tree_create(parent)),
      shell_overlay(wlr_scene_tree_create(parent)),
      layer_popups(wlr_scene_tree_create(parent)),
      seat(wlr_scene_tree_create(parent)) {}

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
