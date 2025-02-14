#pragma once

#include <list>

#include "wlr.hpp"

namespace output {
    struct Output;
}

namespace nodes {
    enum class NodeType { ROOT, OUTPUT, TOPLEVEL };

    template <NodeType type>
    class Node {
        public:
        Node() {
            static int next_id = 1;
            id = next_id++;
            wl_signal_init(&destroy);
        }

        private:
        size_t id;

        wl_signal destroy;
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
    struct Root : Node<NodeType::ROOT> {
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

        wl_signal new_node;
    };
}
