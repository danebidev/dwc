#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "build-config.h"
#include "server.h"

extern "C" {
#include <getopt.h>
}

#include <wlr.h>

__attribute__((noreturn)) void usage() {
    printf("Usage: %s [options]\n", PROGRAM_NAME);
    printf("Options:\n");
    printf("    -h              display this help message\n");
    printf("    -s [command]    start a program on wm startup\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    wlr_log_init(WLR_INFO, NULL);

    int c;
    while((c = getopt(argc, argv, "s:hv")) != -1) {
        switch(c) {
            case 'h':
                usage();
            case 'v':
                wlr_log_init(WLR_DEBUG, NULL);
                break;
            default:
                usage();
        }
    }

    if(optind < argc)
        usage();

    try {
        Server server;
        server.start();
    }
    catch(const std::runtime_error &err) {
        wlr_log(WLR_ERROR, "%s", err.what());
        return EXIT_FAILURE;
    }
}
