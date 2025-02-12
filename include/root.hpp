#pragma once

#include <variant>

#include "wlr.hpp"

namespace nodes {
    // scene layout (from top to bottom):
    // - root
    //   - seat stuff
    //   - layer popups
    //     - [output trees]
    //   - shell_overlay
    //     - [output trees]
    //   - shell_top
    //     - [output trees]
    //   - toplevel popups
    //   - floating
    //   - shell_bottom
    //     - [output trees]
    //   - shell_background
    //     - [output trees]
    struct Root {
        wlr_scene_tree* shell_background;
        wlr_scene_tree* shell_bottom;
        wlr_scene_tree* floating;
        wlr_scene_tree* toplevel_popups;
        wlr_scene_tree* shell_top;
        wlr_scene_tree* shell_overlay;
        wlr_scene_tree* layer_popups;
        wlr_scene_tree* seat;

        Root(wlr_scene_tree* parent);

        void arrange();

        struct {
            wl_signal new_node;
        } events;
    };
}
