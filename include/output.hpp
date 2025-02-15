#pragma once

#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace output {
    void new_output(wl_listener* listener, void* data);

    class Output {
        friend void frame(wl_listener*, void*);
        friend void request_state(wl_listener*, void*);
        friend void destroy(wl_listener* listener, void* data);

        public:
        wlr_output* output;
        wlr_scene_output* scene_output;

        int lx, ly;

        struct {
            wlr_scene_tree* shell_background;
            wlr_scene_tree* shell_bottom;
            wlr_scene_tree* shell_top;
            wlr_scene_tree* shell_overlay;
        } layers;

        Output(wlr_output* output);

        void update_position();
        void arrange_layers();

        wlr_scene_tree* get_scene(zwlr_layer_shell_v1_layer layer);

        private:
        wrapper::Listener<Output> frame;
        wrapper::Listener<Output> request_state;
        wrapper::Listener<Output> destroy;

        void arrange_surface(wlr_box* full_area, wlr_box* usable_area, wlr_scene_tree* tree,
                             bool exclusive);
    };
}
