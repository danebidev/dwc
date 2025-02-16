#include "config/config.hpp"

#include <unistd.h>
#include <wordexp.h>

#include <fstream>
#include <sstream>

#include "config/parser.hpp"

config::Config conf;

extern char** environ;

namespace config {
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

        parser::Parser parser(text);
        std::vector<statements::Statement*> statements = parser.parse();

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

        for(auto& statement : statements) {
            switch(statement->type) {
                case statements::StatementType::SET:
                    command_set(static_cast<statements::SetStatement*>(statement));
                    break;
                case statements::StatementType::ENV:
                    command_env(static_cast<statements::EnvStatement*>(statement));
                    break;
                case statements::StatementType::EXEC:
                    command_exec(static_cast<statements::ExecStatement*>(statement));
                    break;
                case statements::StatementType::EXEC_ALWAYS:
                    command_exec_always(static_cast<statements::ExecAlwaysStatement*>(statement));
                    break;
                case statements::StatementType::OUTPUT:
                    command_output(static_cast<statements::OutputStatement*>(statement));
                    break;
            }
        }
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

    void Config::command_set(statements::SetStatement* statement) {
        vars[statement->name] = statement->content.str(vars);
        delete statement;
    }

    void Config::command_env(statements::EnvStatement* statement) {
        setenv(statement->name.c_str(), statement->content.str(vars).c_str(), true);
        vars[statement->name] = statement->content.str(vars);
        delete statement;
    }

    void Config::command_exec(statements::ExecStatement* statement) {
        exec.push_back(statement->content.str(vars));
        delete statement;
    }

    void Config::command_exec_always(statements::ExecAlwaysStatement* statement) {
        exec_always.push_back(statement->content.str(vars));
        delete statement;
    }

    void Config::command_output(statements::OutputStatement* statement) {
        // TODO
        delete statement;
    }
}
