#pragma once

#include "wlr.h"

namespace output {
    struct Output {
        wlr_output* output;
        wl_listener frame;
        wl_listener request_state;
        wl_listener destroy;
    };

    void new_output(wl_listener* listener, void* data);

    void frame(wl_listener* listener, void* data);
    void request_state(wl_listener* listener, void* data);
    void destroy(wl_listener* listener, void* data);
}
