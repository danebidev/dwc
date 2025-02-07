#pragma once

#include "wlr.hpp"
#include "wlr_wrappers.hpp"

namespace output {
    struct Output {
        wlr_output* output;

        wrapper::Listener<Output> frame;
        wrapper::Listener<Output> request_state;
        wrapper::Listener<Output> destroy;

        Output(wlr_output* output);
    };

    // Called when a new output (monitor or display) becomes available
    // data is a wlr_output*
    void new_output(wl_listener* listener, void* data);

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
