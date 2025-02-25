#pragma once

#include <list>
#include <unordered_map>

#include "wlr.hpp"

namespace workspace {
    class Workspace;
}

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
        struct {
            // Called with the destroyed nodes::Node as data
            wl_signal node_destroy;
        } events;

        NodeType type;

        union {
            xdg_shell::Toplevel* toplevel;
            layer_shell::LayerSurface* layer_surface;
        } val;

        Node(xdg_shell::Toplevel* toplevel);
        Node(layer_shell::LayerSurface* layer_surface);

        // Always false for toplevels
        bool has_exclusivity();
    };

    // scene layout (from top to bottom):
    // root
    //      - seat
    //      - fullscreen
    //          - [workspaces fullscreen tree]
    //      - layer popups
    //          - [layer surfaces popup tree]
    //      - shell_overlay
    //          - [outputs shell_overlay]
    //              - [layer surfaces]
    //      - shell_top
    //          - [outputs shell_top]
    //              - [layer surfaces]
    //      - floating
    //          - [floating windows]
    //      - shell_bottom
    //          - [outputs shell_bottom]
    //              - [layer surfaces]
    //      - shell_background
    //          - [outputs shell_background]
    //              - [layer surfaces]

    class Root {
        public:
        wlr_scene* scene;
        wlr_output_layout* output_layout;

        wlr_scene_tree* shell_background;
        wlr_scene_tree* shell_bottom;
        wlr_scene_tree* floating;
        wlr_scene_tree* shell_top;
        wlr_scene_tree* shell_overlay;
        wlr_scene_tree* layer_popups;
        wlr_scene_tree* fullscreen;
        wlr_scene_tree* seat;

        std::unordered_map<int, workspace::Workspace*> workspaces;

        struct {
            // Called with the new nodes::Node as data
            wl_signal new_node;
        } events;

        Root(wl_display* display);

        void arrange();

        private:
    };
}
