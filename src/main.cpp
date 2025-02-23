#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "build-config.h"
#include "config/config.hpp"
#include "server.hpp"

extern "C" {
#include <getopt.h>
}

#include <wlr.hpp>

void usage() {
    printf("Usage: %s [options]\n", PROGRAM_NAME);
    printf("Options:\n");
    printf("    -h              display this help message\n");
    printf("    -v              display debug output\n");
    printf("    -c              set the config file path\n");
}

int main(int argc, char **argv) {
#ifdef DEBUG
    wlr_log_init(WLR_DEBUG, nullptr);
#else
    wlr_log_init(WLR_INFO, nullptr);
#endif
    char *startup_cmd = nullptr;
    char *config_path = nullptr;

    int c;
    while((c = getopt(argc, argv, "c:hv")) != -1) {
        switch(c) {
            case 'h':
                usage();
                exit(0);
            case 'v':
                wlr_log_init(WLR_DEBUG, nullptr);
                break;
            case 'c':
                config_path = optarg;
                break;
            default:
                usage();
                exit(1);
        }
    }

    if(optind < argc) {
        usage();
        exit(1);
    }

    if(config_path)
        conf.set_config_path(config_path);

    conf.load();
    conf.execute_phase(ConfigLoadPhase::CONFIG_FIRST_LOAD);

    try {
        server.start(startup_cmd);
    }
    catch(const std::runtime_error &err) {
        wlr_log(WLR_ERROR, "%s", err.what());
        wlr_log(WLR_ERROR, "Unrecoverable error. Compositor exiting.");
        return EXIT_FAILURE;
    }

    fflush(stdout);
}
