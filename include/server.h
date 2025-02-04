#pragma once

#include "wlr.h"

class Server {
    private:
    wl_display* display;
    wlr_backend* backend;
    wlr_renderer* renderer;

    public:
    Server();
    ~Server();

    void start();
};
