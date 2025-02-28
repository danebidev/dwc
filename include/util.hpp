#pragma once

#include <chrono>
#include <format>
#include <iostream>
#include <string>

#include "wlr.hpp"

#define LISTEN(signal, func) \
    this, &signal, std::bind(&func, this, std::placeholders::_1, std::placeholders::_2)

enum class ConfigLoadPhase { CONFIG_FIRST_LOAD, COMPOSITOR_START, BIND, RELOAD };
enum class LogLevel { TRACE, DEBUG, INFO, WARNING, ERROR };

struct Mode {
    int width;
    int height;
    double refresh_rate;
};

struct Position {
    int x;
    int y;
};

class Logger {
    public:
    LogLevel log_level;

    // Stolen from Hyprland
    template <typename... Args>
    void log(LogLevel level, std::format_string<Args...> fmt, Args &&...args) {
        if(level < log_level)
            return;

        // Get the time since the program start
        static std::chrono::steady_clock::time_point program_start =
            std::chrono::steady_clock::now();
        std::chrono::steady_clock::duration elapsed =
            std::chrono::steady_clock::now() - program_start;

        // Format the time
        std::chrono::milliseconds ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
        std::chrono::minutes m = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
        std::chrono::hours h = std::chrono::duration_cast<std::chrono::hours>(elapsed);

        std::string log_msg = std::format("[{:02}:{:02}:{:02}.{:03}] ", h.count(), m.count() % 60,
                                          s.count() % 60, ms.count() % 1000);
        std::string str = std::vformat(fmt.get(), std::make_format_args(args...));

        switch(level) {
            case LogLevel::TRACE:
                log_msg += "\033[30m[TRACE] " + str += +"\033[0m";  // non-bold gray
                break;
            case LogLevel::DEBUG:
                log_msg += "\033[1;30m[DEBUG] " + str + "\033[0m";  // gray
                break;
            case LogLevel::INFO:
                log_msg += "\033[1;34m[INFO] " + str + "\033[0m";  // green
                break;
            case LogLevel::WARNING:
                log_msg += "\033[1;33m[WARN] " + str + "\033[0m";  // yellow
                break;
            case LogLevel::ERROR:
                log_msg += "\033[1;31m[ERR] " + str + "\033[0m";  // red
                break;
            default:
                break;
        }

        std::cout << log_msg << "\n";
    }
};

extern Logger logger;

void trim(std::string &s);
std::string device_identifier(wlr_input_device *device);
