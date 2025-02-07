#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "build-config.h"
#include "server.hpp"

extern "C" {
#include <getopt.h>
}

#include <wlr.hpp>

__attribute__((noreturn)) void usage() {
    printf("Usage: %s [options]\n", PROGRAM_NAME);
    printf("Options:\n");
    printf("    -h              display this help message\n");
    printf("    -v              display debug output\n");
    printf("    -s [command]    start a program on wm startup\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    wlr_log_init(WLR_INFO, NULL);
    char *startup_cmd = nullptr;

    int c;
    while((c = getopt(argc, argv, "s:hv")) != -1) {
        switch(c) {
            case 'h':
                usage();
            case 'v':
                wlr_log_init(WLR_DEBUG, NULL);
                break;
            case 's':
                startup_cmd = optarg;
                break;
            default:
                usage();
        }
    }

    if(optind < argc)
        usage();

    try {
        Server::instance().start(startup_cmd);
    }
    catch(const std::runtime_error &err) {
        wlr_log(WLR_ERROR, "%s", err.what());
        wlr_log(WLR_ERROR, "Unrecoverable error. Compositor exiting.");
        return EXIT_FAILURE;
    }
}
