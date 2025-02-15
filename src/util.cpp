#include "util.hpp"

#include <algorithm>
#include <format>

#include "wlr.hpp"

inline void ltrim(std::string &s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
}

void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

// Gets the device identifier of a device, sway style
std::string device_identifier(wlr_input_device *device) {
    int vendor = 0, product = 0;
    if(wlr_input_device_is_libinput(device)) {
        libinput_device *libinput_dev = wlr_libinput_get_device_handle(device);
        vendor = libinput_device_get_id_vendor(libinput_dev);
        product = libinput_device_get_id_product(libinput_dev);
    }

    // Apparently the device name can be null (how??)
    std::string name = device->name ? device->name : "";
    trim(name);

    for(auto &c : name) {
        // Device names can contain not-printable characters
        if(c == ' ' || !isprint(c))
            c = '_';
    }

    return std::format("{}:{}:{}", vendor, product, name);
}
