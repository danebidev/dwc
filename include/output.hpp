#pragma once

#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace output {
    void new_output(wl_listener* listener, void* data);

    class Output {
        public:
        wlr_output* output;

        wlr_scene_output* scene_output;
        struct {
            wlr_scene_tree* shell_background;
            wlr_scene_tree* shell_bottom;
            wlr_scene_tree* shell_top;
            wlr_scene_tree* shell_overlay;
        } layers;

        int lx, ly;

        Output(wlr_output* output);

        void arrange_layers();
        void update_position();
        wlr_scene_tree* get_scene(zwlr_layer_shell_v1_layer layer);

        private:
        int width, height;

        wrapper::Listener<Output> frame;
        wrapper::Listener<Output> request_state;
        wrapper::Listener<Output> destroy;

        void arrange_surface(wlr_box* full_area, wlr_box* usable_area, wlr_scene_tree* tree,
                             bool exclusive);
    };
}
