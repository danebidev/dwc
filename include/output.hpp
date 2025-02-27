#pragma once

#include <string>

#include "config/config.hpp"
#include "root.hpp"
#include "wlr-wrapper.hpp"
#include "wlr.hpp"
#include "workspace.hpp"

class Server;

namespace output {
    // Called when a new output (monitor or display) becomes available
    void new_output(Server* listener, void* data);

    class Output {
        public:
        wlr_output* output;
        wlr_scene_output* scene_output;

        wlr_box output_box;
        wlr_box usable_area;

        std::list<workspace::Workspace*> workspaces;
        workspace::Workspace* active_workspace;

        struct {
            wlr_scene_tree* shell_background;
            wlr_scene_tree* shell_bottom;
            wlr_scene_tree* shell_top;
            wlr_scene_tree* shell_overlay;
        } layers;

        Output(wlr_output* output);
        ~Output();

        void update_position();
        void arrange_layers();
        bool apply_config(config::OutputConfig* config, bool test);

        wlr_scene_tree* get_scene(zwlr_layer_shell_v1_layer layer);
        std::pair<double, double> center();

        private:
        wrapper::Listener<Output> frame_list;
        wrapper::Listener<Output> request_state_list;
        wrapper::Listener<Output> destroy_list;

        void arrange_surface(wlr_box* full_area, wlr_scene_tree* tree, bool exclusive);

        // Called whenever an output wants to display a frame
        // Generally should be at the output's refresh rate
        void frame(Output* output, void*);
        // Called when the backend request a new state
        // For example, resizing a window in the X11 or wayland backend
        void request_state(Output* output, void*);
        // Called when an output is destroyed
        void destroy(Output* output, void* data);
    };

    class OutputManager {
        public:
        std::list<Output*> outputs;

        OutputManager(wl_display* display);
        Output* output_at(double x, double y);
        Output* focused_output();

        void apply_output_config(wlr_output_configuration_v1* config, bool test);

        private:
        wrapper::Listener<OutputManager> layout_update_list;
        wrapper::Listener<OutputManager> test_list;
        wrapper::Listener<OutputManager> apply_list;

        // Cleanup listeners
        wrapper::Listener<OutputManager> layout_destroy_list;
        wrapper::Listener<OutputManager> destroy_list;

        void layout_update(OutputManager* manager, void* data);
        void output_test(OutputManager* manager, void* data);
        void output_apply(OutputManager* manager, void* data);

        void layout_destroy(OutputManager* manager, void* data);
        void destroy(OutputManager* manager, void* data);
    };
}
