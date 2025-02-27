#include "config/config.hpp"

#include <unistd.h>
#include <wordexp.h>

#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>

#include "output.hpp"

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

    Bind::Bind(uint32_t modifiers, xkb_keysym_t sym)
        : modifiers(modifiers),
          sym(sym) {}

    std::optional<Bind> Bind::from_str(int line, std::string str) {
        uint32_t modifiers = 0;
        xkb_keysym_t bind_sym = XKB_KEY_NoSymbol;

        std::string token;
        std::stringstream ss(str);

        while(std::getline(ss, token, '+')) {
            if(token == "Number") {
                bind_sym = XKB_KEY_NoSymbol;
                continue;
            }

            xkb_keysym_t sym = xkb_keysym_from_name(token.c_str(), XKB_KEYSYM_NO_FLAGS);

            if(sym == XKB_KEY_NoSymbol) {
                uint32_t modifier = parse_modifier(token);

                if(modifier == INVALID_MODIFIER) {
                    wlr_log(WLR_ERROR, "Error on line %d: no such keycode or modifier '%s'", line,
                            token.c_str());
                    return std::nullopt;
                }

                modifiers |= modifier;
            }
            else
                bind_sym = sym;
        }

        return Bind(modifiers, bind_sym);
    }

#undef INVALID_MODIFIER

    OutputConfig::OutputConfig()
        : enabled(true),
          mode(std::nullopt),
          pos({ 0, 0 }),
          transform(WL_OUTPUT_TRANSFORM_NORMAL),
          scale(1.0),
          adaptive_sync(false) {}

    OutputConfig::OutputConfig(wlr_output_configuration_head_v1* config)
        : enabled(config->state.enabled),
          pos({ config->state.x, config->state.y }),
          transform(config->state.transform),
          scale(config->state.scale),
          adaptive_sync(config->state.adaptive_sync_enabled) {
        if(config->state.mode) {
            wlr_output_mode* s = config->state.mode;
            mode = Mode { .width = s->width,
                          .height = s->height,
                          .refresh_rate = s->refresh / 1000.f };
        }
        else {
            auto& s = config->state.custom_mode;
            mode =
                Mode { .width = s.width, .height = s.height, .refresh_rate = s.refresh / 1000.f };
        }
    }

    Config::~Config() {
        clear();
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

    void Config::execute_phase(ConfigLoadPhase phase) {
        for(auto& command : commands) {
            if(!command->execute(phase))
                break;
        }
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

    void Config::clear() {
        for(auto& command : commands) {
            delete command;
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
