#pragma once

#include <string>

#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace output {
    // Called when a new output (monitor or display) becomes available
    void new_output(wl_listener* listener, void* data);
    void layout_update(wl_listener* listener, void* data);
    void apply_output_config(wlr_output_configuration_v1* config, bool test);

    class OutputConfig {
        public:
        std::string name;
        bool enabled;
        int32_t width, height;
        double x, y;
        double refresh;
        enum wl_output_transform transform;
        double scale;
        bool adaptive_sync;

        OutputConfig(wlr_output_configuration_head_v1* config);
    };

    class Output {
        friend void frame(wl_listener*, void*);
        friend void request_state(wl_listener*, void*);
        friend void destroy(wl_listener* listener, void* data);

        public:
        wlr_output* output;
        wlr_scene_output* scene_output;

        wlr_box output_box;

        struct {
            wlr_scene_tree* shell_background;
            wlr_scene_tree* shell_bottom;
            wlr_scene_tree* shell_top;
            wlr_scene_tree* shell_overlay;
        } layers;

        Output(wlr_output* output);

        void update_position();
        void arrange_layers();

        bool apply_config(OutputConfig* config, bool test);

        wlr_scene_tree* get_scene(zwlr_layer_shell_v1_layer layer);

        private:
        wrapper::Listener<Output> frame;
        wrapper::Listener<Output> request_state;
        wrapper::Listener<Output> destroy;

        void arrange_surface(wlr_box* full_area, wlr_box* usable_area, wlr_scene_tree* tree,
                             bool exclusive);
    };
}
