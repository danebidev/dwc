#pragma once

#include <string>

#include "wlr.hpp"

enum class ConfigLoadPhase { CONFIG_FIRST_LOAD, COMPOSITOR_START, BIND, RELOAD };

void trim(std::string &s);
std::string device_identifier(wlr_input_device *device);
