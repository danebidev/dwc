#pragma once

#include <vector>

#include "output.h"
#include "wlr.h"

class Server {
    private:
    wl_listener new_output_listener;

    Server();
    ~Server();

    public:
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;
    wlr_allocator* allocator;

    wlr_output_layout* output_layout;
    wlr_scene* scene;
    wlr_scene_output_layout* scene_layout;

    std::vector<output::Output*> outputs;

    static Server& instance();

    void start();
};
