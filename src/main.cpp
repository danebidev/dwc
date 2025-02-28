#include <getopt.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <wlr.hpp>

#include "build-config.h"
#include "config/config.hpp"
#include "server.hpp"

void usage() {
    printf("Usage: %s [options]\n", PROGRAM_NAME);
    printf("Options:\n");
    printf("    -h              display this help message\n");
    printf("    -v level        set the debug level from 0 (debug) to 3 (error)\n");
    printf("    -c              set the config file path\n");
}

void log_wlr(wlr_log_importance importance, const char *fmt, va_list args) {
    LogLevel level;
    switch(importance) {
        case WLR_ERROR:
            level = LogLevel::ERROR;
            break;
        case WLR_INFO:
            level = LogLevel::INFO;
            break;
        case WLR_DEBUG:
            level = LogLevel::DEBUG;
            break;
        default:
            level = LogLevel::TRACE;
            break;
    }

    // The whole thing is kinda hacky
    va_list args_copy;
    va_copy(args_copy, args);

    int size = vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if(size < 0) {
        return;
    }

    std::string formatted_message;
    formatted_message.resize(size);

    vsnprintf(&formatted_message[0], size + 1, fmt, args);
    logger.log(level, "{}", formatted_message);
}

int main(int argc, char **argv) {
#ifdef DEBUG_BUILD
    wlr_log_init(WLR_DEBUG, log_wlr);
    logger.log_level = LogLevel::TRACE;
#else
    wlr_log_init(WLR_INFO, log_wlr);
    logger.log_level = LogLevel::INFO;
#endif
    char *startup_cmd = nullptr;
    char *config_path = nullptr;

    int c;
    while((c = getopt(argc, argv, "c:hv:")) != -1) {
        switch(c) {
            case 'h':
                usage();
                exit(0);
            case 'v':
                try {
                    if(logger.log_level == LogLevel::TRACE)
                        break;
                    int level = std::stoi(optarg);
                    if(level < 0 || level > 3) {
                        logger.log(LogLevel::ERROR, "invalid loglevel (0-3)");
                        exit(EXIT_FAILURE);
                    }
                    logger.log_level = static_cast<LogLevel>(level + 1);
                    if(level == 0)
                        wlr_log_init(WLR_DEBUG, log_wlr);
                }
                catch(const std::runtime_error &err) {
                    logger.log(LogLevel::ERROR, "loglevel isn't a valid number");
                    exit(EXIT_FAILURE);
                }
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
        logger.log(LogLevel::ERROR, "{}", err.what());
        logger.log(LogLevel::ERROR, "error during initialization");
        return EXIT_FAILURE;
    }

    fflush(stdout);
}
