#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/parser.hpp"
#include "wlr.hpp"

namespace config {
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

        std::vector<std::string> exec;
        std::vector<std::string> exec_always;

        std::vector<std::pair<std::string, std::string>> env;

        private:
        std::filesystem::path config_path;
        std::string text;

        std::unordered_map<std::string, std::string> vars;

        void default_config_path();
        std::string read_file();

        void command_set(statements::SetStatement *statement);
        void command_env(statements::EnvStatement *statement);
        void command_exec(statements::ExecStatement *statement);
        void command_exec_always(statements::ExecAlwaysStatement *statement);
        void command_output(statements::OutputStatement *statement);
    };
}

extern config::Config conf;
