#pragma once

#include <string>

#include "config/config.hpp"
#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace output {
    // Called when a new output (monitor or display) becomes available
    void new_output(wl_listener* listener, void* data);
    void layout_update(wl_listener* listener, void* data);

    class Output {
        friend void frame(wl_listener*, void*);
        friend void request_state(wl_listener*, void*);
        friend void output_destroy(wl_listener* listener, void* data);

        public:
        wlr_output* output;
        wlr_scene_output* scene_output;

        wlr_box output_box;
        wlr_box usable_area;

        struct {
            wlr_scene_tree* shell_background;
            wlr_scene_tree* shell_bottom;
            wlr_scene_tree* shell_top;
            wlr_scene_tree* shell_overlay;
        } layers;

        Output(wlr_output* output);

        void update_position();
        void arrange_layers();
        bool apply_config(config::OutputConfig* config, bool test);

        wlr_scene_tree* get_scene(zwlr_layer_shell_v1_layer layer);

        private:
        wrapper::Listener<Output> frame;
        wrapper::Listener<Output> request_state;
        wrapper::Listener<Output> destroy;

        void arrange_surface(wlr_box* full_area, wlr_scene_tree* tree, bool exclusive);
    };

    class OutputManager {
        friend void output_layout_destroy(wl_listener*, void*);
        friend void output_manager_destroy(wl_listener*, void*);

        public:
        std::list<Output*> outputs;

        OutputManager(wl_display* display);

        Output* output_at(double x, double y);
        Output* focused_output();

        void apply_output_config(wlr_output_configuration_v1* config, bool test);

        private:
        wrapper::Listener<OutputManager> layout_update;
        wrapper::Listener<OutputManager> output_test;
        wrapper::Listener<OutputManager> output_apply;

        wrapper::Listener<OutputManager> output_layout_destroy;
        wrapper::Listener<OutputManager> output_manager_destroy;
    };
}
