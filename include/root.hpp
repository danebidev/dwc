#pragma once

#include <list>

#include "wlr.hpp"

namespace output {
    class Output;
}

namespace layer_shell {
    class LayerSurface;
}

namespace xdg_shell {
    class Toplevel;
}

namespace nodes {
    enum class NodeType { TOPLEVEL, LAYER_SURFACE };

    class Node {
        public:
        NodeType type;
        union {
            xdg_shell::Toplevel* toplevel;
            layer_shell::LayerSurface* layer_surface;
        } val;

        Node(xdg_shell::Toplevel* toplevel);
        Node(layer_shell::LayerSurface* layer_surface);

        // Always false for toplevels
        bool has_exclusivity();

        struct {
            // Called with the destroyed nodes::Node as data
            wl_signal node_destroy;
        } events;
    };

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
        wlr_scene* scene;

        wlr_scene_tree* shell_background;
        wlr_scene_tree* shell_bottom;
        wlr_scene_tree* floating;
        wlr_scene_tree* toplevel_popups;
        wlr_scene_tree* shell_top;
        wlr_scene_tree* shell_overlay;
        wlr_scene_tree* layer_popups;
        wlr_scene_tree* seat;

        wlr_output_layout* output_layout;
        std::list<output::Output*> outputs;

        Root(wl_display* display);

        void arrange();

        struct {
            // Called with the new nodes::Node as data
            wl_signal new_node;
        } events;
    };
}
