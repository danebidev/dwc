#pragma once

#include <string>

#include "wlr.hpp"

#define LISTEN(signal, func) \
    this, &signal, std::bind(&func, this, std::placeholders::_1, std::placeholders::_2)

enum class ConfigLoadPhase { CONFIG_FIRST_LOAD, COMPOSITOR_START, BIND, RELOAD };

struct Mode {
    int width;
    int height;
    double refresh_rate;
};

struct Position {
    int x;
    int y;
};

void trim(std::string &s);
std::string device_identifier(wlr_input_device *device);
