#include "server.h"

#include <stdexcept>

#include "wlr.h"

Server::Server() {
    display = wl_display_create();

    backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL);
    if(backend == nullptr)
        throw std::runtime_error("failed to create wlr_backend");

    renderer = wlr_renderer_autocreate(backend);
    if(renderer == nullptr)
        throw std::runtime_error("failed to create wlr_renderer");
}

Server::~Server() {
    wl_display_destroy(display);
}

void Server::start() {
    wlr_log(WLR_INFO, "Running compositor");
    wl_display_run(display);
}
