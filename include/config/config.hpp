#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/commands.hpp"
#include "util.hpp"
#include "wlr.hpp"

namespace config {
    struct Bind {
        uint32_t modifiers;
        xkb_keysym_t sym;

        Bind(uint32_t modifiers, xkb_keysym_t sym);

        bool operator==(const Bind &other) {
            return modifiers == other.modifiers && sym == other.sym;
        }

        static std::optional<Bind> from_str(int line, std::string text);
    };

    class OutputConfig {
        public:
        std::string name;
        bool enabled;
        int32_t width, height;
        double x, y;
        double refresh;
        enum wl_output_transform transform;
        double scale;
        bool adaptive_sync;

        OutputConfig(wlr_output_configuration_head_v1 *config);
    };

    class Config {
        public:
        void set_config_path(std::filesystem::path path);
        void load();
        void execute_phase(ConfigLoadPhase phase);

        std::unordered_map<std::string, std::string> vars;
        std::vector<std::pair<Bind, commands::Command *>> binds;

        std::vector<commands::Command *> commands;

        ~Config();

        void clear();

        private:
        std::filesystem::path config_path;

        void default_config_path();
        std::string read_file();
    };
}

extern config::Config conf;
