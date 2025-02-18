#include "config/config.hpp"

#include <unistd.h>
#include <wordexp.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>

config::Config conf;

extern char** environ;

namespace config {
#define INVALID_MODIFIER (1 << 8)

    uint32_t parse_modifier(std::string modifier) {
        std::transform(modifier.begin(), modifier.end(), modifier.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        const std::string modifiers[] = {
            "shift", "caps", "ctrl", "alt", "mod2", "mod3", "super", "mod5",
        };

        for(int i = 0; i != 8; ++i)
            if(modifier == modifiers[i])
                return 1 << i;

        return INVALID_MODIFIER;
    }

    Bind::Bind()
        : modifiers(0) {}

    Bind* Bind::from_str(int line, std::string str) {
        Bind* bind = new Bind;

        std::string token;
        std::stringstream ss(str);

        while(std::getline(ss, token, '+')) {
            if(token == "Number") {
                bind->sym = XKB_KEY_NoSymbol;
                continue;
            }

            xkb_keysym_t sym = xkb_keysym_from_name(token.c_str(), XKB_KEYSYM_NO_FLAGS);

            if(sym == XKB_KEY_NoSymbol) {
                uint32_t modifier = parse_modifier(token);

                if(modifier == INVALID_MODIFIER) {
                    wlr_log(WLR_ERROR, "Error on line %d: no such keycode or modifier '%s'", line,
                            token.c_str());
                    delete bind;
                    return nullptr;
                }

                bind->modifiers |= modifier;
            }
            else
                bind->sym = sym;
        }

        return bind;
    }

#undef INVALID_MODIFIER

    OutputConfig::OutputConfig(wlr_output_configuration_head_v1* config)
        : enabled(true),
          width(0),
          height(0),
          x(0.0),
          y(0.0),
          refresh(0.0),
          transform(WL_OUTPUT_TRANSFORM_NORMAL),
          scale(1.0),
          adaptive_sync(false) {
        enabled = config->state.enabled;

        if(config->state.mode != nullptr) {
            struct wlr_output_mode* mode = config->state.mode;
            width = mode->width;
            height = mode->height;
            refresh = mode->refresh / 1000.f;
        }
        else {
            width = config->state.custom_mode.width;
            height = config->state.custom_mode.height;
            refresh = config->state.custom_mode.refresh / 1000.f;
        }
        x = config->state.x;
        y = config->state.y;
        transform = config->state.transform;
        scale = config->state.scale;
        adaptive_sync = config->state.adaptive_sync_enabled;
    }

    void Config::set_config_path(std::filesystem::path path) {
        wordexp_t p;
        wordexp(path.c_str(), &p, 0);

        if(p.we_wordc)
            config_path = p.we_wordv[0];
        else
            wlr_log(WLR_ERROR, "config path is not valid - skipping");

        wordfree(&p);
    }

    void Config::load() {
        std::string text;
        if((text = read_file()).empty())
            return;

        parsing::Parser parser(text);
        commands = parser.parse();

        char** env = environ;
        for(; *env; ++env) {
            std::string cur(*env);
            size_t pos = cur.find('=');
            if(pos == cur.size() || pos == 0)
                continue;

            std::string name = cur.substr(pos);
            std::string value = cur.substr(pos + 1, cur.size());
            vars[name] = value;
        }
    }

    void Config::execute_phase(ConfigLoadPhase phase) {
        for(auto& command : commands) {
            if(!command->execute(phase))
                break;
        }
    }

    Config::~Config() {
        clear();
    }

    void Config::clear() {
        for(auto& command : commands) {
            delete command;
        }

        for(auto& bind : binds) {
            delete bind.first;
        }

        commands.clear();
        vars.clear();
        binds.clear();
    }

    void Config::default_config_path() {
        std::filesystem::path path;
        const char* config_home = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");

        // env and tilde will be expanded later
        if(config_home && *config_home)
            path = std::filesystem::path(config_home) / "dwc/config";
        else if(home && *home)
            path = std::filesystem::path(home) / ".config/dwc/config";
        else
            path = "~/.config/dwc/config";

        set_config_path(path);
    }

    std::string Config::read_file() {
        if(config_path.empty())
            default_config_path();

        wlr_log(WLR_INFO, "reading config file at %s", config_path.c_str());
        if(access(config_path.c_str(), R_OK)) {
            wlr_log(WLR_ERROR, "failed reading config file - skipping");
            return "";
        }

        std::ifstream input(config_path, std::ios::binary);

        if(!input.is_open()) {
            wlr_log(WLR_ERROR, "failed reading config file - skipping");
            return "";
        }

        std::ostringstream sstr;
        sstr << input.rdbuf();
        input.close();

        return sstr.str();
    }
}
