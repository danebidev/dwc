#pragma once

#include "wlr-wrapper.hpp"
#include "wlr.hpp"

namespace output {
    struct Output {
        wlr_output* output;

        struct {
            wlr_scene_tree* shell_background;
            wlr_scene_tree* shell_bottom;
            wlr_scene_tree* shell_top;
            wlr_scene_tree* shell_overlay;
        } layers;

        wlr_scene_output* scene_output;

        int lx, ly;
        int width, height;

        wrapper::Listener<Output> frame;
        wrapper::Listener<Output> request_state;
        wrapper::Listener<Output> destroy;

        Output(wlr_output* output);

        void arrange_surface(wlr_box* full_area, wlr_box* usable_area, wlr_scene_tree* tree,
                             bool exclusive);
        void arrange();

        void update_position();
    };

    // Called when a new output (monitor or display) becomes available
    // data is a wlr_output*
    void new_output(wl_listener* listener, void* data);

    void arrange_outputs();

    // Called whenever an output wants to display a frame
    // Generally should be at the output's refresh rate
    void frame(wl_listener* listener, void* data);

    // Called when the backend request a new state
    // For example, resizing a window in the X11 or wayland backend
    // data is a wlr_output_event_request_state*
    void request_state(wl_listener* listener, void* data);

    // Called when an output is destroyed
    void destroy(wl_listener* listener, void* data);
}
