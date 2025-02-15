#pragma once

#include <string>

#include "wlr.hpp"

void trim(std::string &s);

std::string device_identifier(wlr_input_device *device);
