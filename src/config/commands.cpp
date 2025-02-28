#include "config/commands.hpp"

#include <unistd.h>

#include <cassert>
#include <regex>

#include "config/config.hpp"
#include "server.hpp"
#include "wlr.hpp"

commands::Command* parse_command(std::string name, int line, std::vector<std::string> args) {
    if(name == "set")
        return commands::SetCommand::parse(line, args);
    else if(name == "env")
        return commands::EnvCommand::parse(line, args);
    else if(name == "exec")
        return commands::ExecCommand::parse(line, args);
    else if(name == "exec_always")
        return commands::ExecAlwaysCommand::parse(line, args);
    else if(name == "output")
        return commands::OutputCommand::parse(line, args);
    else if(name == "bind")
        return commands::BindCommand::parse(line, args);
    else if(name == "terminate")
        return commands::TerminateCommand::parse(line, args);
    else if(name == "reload")
        return commands::ReloadCommand::parse(line, args);
    else if(name == "kill")
        return commands::KillCommand::parse(line, args);
    else if(name == "workspace")
        return commands::WorkspaceCommand::parse(line, args);
    else if(name == "fullscreen")
        return commands::FullscreenCommand::parse(line, args);
    else if(name == "debug")
        return commands::DebugCommand::parse(line, args);
    else {
        logger.log(LogLevel::ERROR, "Error on line {}: command '{}' not recognized", line, name);
        return nullptr;
    }
}

std::vector<commands::Command*> parse(int line, std::string name, std::vector<std::string> args,
                                      std::optional<std::vector<std::vector<std::string>>> block) {
    if(!block.has_value())
        return { parse_command(name, line, args) };

    std::vector<commands::Command*> commands;
    for(const auto& subcommand : block.value()) {
        // Slow af, but who cares
        std::vector<std::string> new_args = args;
        for(auto& arg : subcommand) {
            new_args.push_back(arg);
        }
        commands.push_back(parse_command(name, ++line, new_args));
    }

    return commands;
}

namespace commands {
    std::string ParsableContent::str(std::unordered_map<std::string, std::string>& envs) {
        std::string result;
        std::string currentVar;
        bool inVariable = false;

        for(size_t i = 0; i < content.size(); i++) {
            char c = content[i];

            if(inVariable) {
                if(std::isalnum(c))
                    currentVar += c;
                else {
                    result += envs[currentVar] + c;
                    currentVar.clear();
                    if(c != '$')
                        inVariable = false;
                }
            }
            else if(c == '$') {
                inVariable = true;
                currentVar.clear();
            }
            else
                result += c;
        }

        if(inVariable)
            result += envs[currentVar];

        return result;
    }

    ParsableContent::ParsableContent(std::string content)
        : content(content) {}

    Command::Command(int line, CommandType type, bool subcommand_only)
        : line(line),
          type(type),
          subcommand_only(subcommand_only) {}

    SetCommand::SetCommand(int line, std::string name, ParsableContent content)
        : Command(line, CommandType::SET, false),
          name(name),
          content(content) {}

    SetCommand* SetCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            logger.log(LogLevel::ERROR, "Error on line {}: missing set argument", line);
            return nullptr;
        }
        std::string value;
        for(size_t i = 1; i < args.size(); i++) {
            value += args[i] + (i == args.size() - 1 ? "" : " ");
        }

        return new SetCommand(line, args[0], ParsableContent(value));
    }

    bool SetCommand::subcommand_of(CommandType type) {
        return false;
    }

    bool SetCommand::execute(ConfigLoadPhase phase) {
        // Only set vars on config first load and reloads
        if(phase == ConfigLoadPhase::COMPOSITOR_START)
            return true;
        conf.vars[name] = content.str(conf.vars);

        return true;
    }

    EnvCommand::EnvCommand(int line, std::string name, ParsableContent content)
        : Command(line, CommandType::ENV, false),
          name(name),
          content(content) {}

    EnvCommand* EnvCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            logger.log(LogLevel::ERROR, "Error on line {}: missing env argument", line);
            return nullptr;
        }
        std::string value;
        for(size_t i = 1; i < args.size(); i++) {
            value += args[i] + (i == args.size() - 1 ? "" : " ");
        }

        return new EnvCommand(line, args[0], ParsableContent(value));
    }

    bool EnvCommand::subcommand_of(CommandType type) {
        return false;
    }

    bool EnvCommand::execute(ConfigLoadPhase phase) {
        // Only set envs once, they don't get reloaded
        if(phase != ConfigLoadPhase::CONFIG_FIRST_LOAD)
            return true;
        setenv(name.c_str(), content.str(conf.vars).c_str(), true);
        conf.vars[name] = content.str(conf.vars);
        return true;
    }

    ExecCommand::ExecCommand(int line, ParsableContent content)
        : Command(line, CommandType::EXEC, false),
          content(content) {}

    ExecCommand* ExecCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            logger.log(LogLevel::ERROR, "Error on line {}: missing exec argument", line);
            return nullptr;
        }
        std::string value;
        for(size_t i = 0; i < args.size(); i++) {
            value += args[i] + (i == args.size() - 1 ? "" : " ");
        }

        return new ExecCommand(line, ParsableContent(value));
    }

    bool ExecCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool ExecCommand::execute(ConfigLoadPhase phase) {
        // execs only get executed on first start
        if(phase != ConfigLoadPhase::COMPOSITOR_START && phase != ConfigLoadPhase::BIND)
            return true;
        int pid = fork();
        if(pid < 0) {
            logger.log(LogLevel::ERROR, "fork() failed - exiting to avoid errors");
            exit(EXIT_FAILURE);
        }

        if(pid == 0)
            execl("/bin/sh", "/bin/sh", "-c", content.str(conf.vars).c_str(), (void*)nullptr);
        return true;
    }

    ExecAlwaysCommand::ExecAlwaysCommand(int line, ParsableContent content)
        : Command(line, CommandType::EXEC_ALWAYS, false),
          content(content) {}

    ExecAlwaysCommand* ExecAlwaysCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            logger.log(LogLevel::ERROR, "Error on line {}: missing exec_always argument", line);
            return nullptr;
        }
        std::string value;
        for(size_t i = 0; i < args.size(); i++) {
            value += args[i] + (i == args.size() - 1 ? "" : " ");
        }

        return new ExecAlwaysCommand(line, ParsableContent(value));
    }

    bool ExecAlwaysCommand::subcommand_of(CommandType type) {
        return false;
    }

    bool ExecAlwaysCommand::execute(ConfigLoadPhase phase) {
        if(phase == ConfigLoadPhase::CONFIG_FIRST_LOAD)
            return true;

        int pid = fork();
        if(pid < 0) {
            logger.log(LogLevel::ERROR, "fork() failed - exiting to avoid errors");
            exit(EXIT_FAILURE);
        }

        if(pid == 0)
            execl("/bin/sh", "/bin/sh", "-c", content.str(conf.vars).c_str(), (void*)nullptr);

        return true;
    }

    OutputCommand::OutputCommand(int line, ParsableContent output_name, std::optional<bool> enabled,
                                 std::optional<Mode> mode, std::optional<Position> position,
                                 std::optional<wl_output_transform> transform,
                                 std::optional<double> scale, std::optional<bool> adaptive_sync)
        : Command(line, CommandType::OUTPUT, false),
          output_name(output_name),
          enabled(enabled),
          mode(mode),
          position(position),
          transform(transform),
          scale(scale),
          adaptive_sync(adaptive_sync) {}

    bool is_number(const std::string& s) {
        return !s.empty() && std::find_if(s.begin(), s.end(), [](unsigned char c) {
                                 return !std::isdigit(c);
                             }) == s.end();
    }

    bool is_double_number(const std::string& s) {
        char* end = nullptr;
        double val = strtod(s.c_str(), &end);
        return end != s.c_str() && *end == '\0' && val != HUGE_VAL;
    }

    OutputCommand* OutputCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() <= 2) {
            if(args.size() == 0)
                logger.log(LogLevel::ERROR, "Error on line {}: missing output name", line);
            else if(args.size() == 1)
                logger.log(LogLevel::ERROR, "Error on line {}: missing output subcommand", line);
            else if(args.size() == 2)
                logger.log(LogLevel::ERROR, "Error on line {}: missing argument to {} subcommand",
                           line, args[1]);
            return nullptr;
        }

        std::smatch m;

        if(args[1] == "enable") {
            if(args.size() > 3) {
                logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
                return nullptr;
            }

            if(args[2] == "on")
                return new OutputCommand(line, args[0], true);

            else if(args[2] == "off")
                return new OutputCommand(line, args[0], false);
            else {
                logger.log(LogLevel::ERROR, "Error on line {}: invalid enable argument", line);
                return nullptr;
            }
        }
        if(args[1] == "mode") {
            if(args.size() > 3) {
                logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
                return nullptr;
            }

            std::regex re("(\\d+)x(\\d+)(@(\\d+(\\.\\d+)?)Hz)?");
            if(std::regex_match(args[2], m, re)) {
                int width = stoi(m[1]);
                int height = stoi(m[2]);
                double refresh;
                try {
                    refresh = stod(m[4]);
                }
                catch(std::runtime_error& e) {
                    refresh = 60;
                }
                Mode mode { .width = width, .height = height, .refresh_rate = refresh };
                return new OutputCommand(line, args[0], std::nullopt, mode);
            }
            else {
                logger.log(LogLevel::ERROR, "Error on line {}: invalid mode argument", line);
                return nullptr;
            }
        }
        else if(args[1] == "position") {
            if(args.size() > 4) {
                logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
                return nullptr;
            }

            if(is_number(args[2]) && is_number(args[3])) {
                Position pos { .x = stoi(args[2]), .y = stoi(args[3]) };
                return new OutputCommand(line, args[0], std::nullopt, std::nullopt, pos);
            }
            else {
                logger.log(LogLevel::ERROR, "Error on line {}: invalid position argument", line);
                return nullptr;
            }
        }
        else if(args[1] == "transform") {
            if(args.size() > 3) {
                logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
                return nullptr;
            }

            wl_output_transform transform;
            if(args[2] == "normal")
                transform = WL_OUTPUT_TRANSFORM_NORMAL;
            else if(args[2] == "90")
                transform = WL_OUTPUT_TRANSFORM_90;
            else if(args[2] == "180")
                transform = WL_OUTPUT_TRANSFORM_180;
            else if(args[2] == "270")
                transform = WL_OUTPUT_TRANSFORM_270;
            else if(args[2] == "flipped")
                transform = WL_OUTPUT_TRANSFORM_FLIPPED;
            else if(args[2] == "flipped-90")
                transform = WL_OUTPUT_TRANSFORM_FLIPPED_90;
            else if(args[2] == "flipped-180")
                transform = WL_OUTPUT_TRANSFORM_FLIPPED_180;
            else if(args[2] == "flipped-270")
                transform = WL_OUTPUT_TRANSFORM_FLIPPED_270;
            else {
                logger.log(LogLevel::ERROR, "Error on line {}: invalid transform argument", line);
                return nullptr;
            }

            return new OutputCommand(line, args[0], std::nullopt, std::nullopt, std::nullopt,
                                     transform);
        }
        else if(args[1] == "scale") {
            if(args.size() > 3) {
                logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
                return nullptr;
            }

            if(is_double_number(args[2]))
                return new OutputCommand(line, args[0], std::nullopt, std::nullopt, std::nullopt,
                                         std::nullopt, stod(args[2]));
        }
        else if(args[1] == "adaptive_sync") {
            if(args.size() > 3) {
                logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
                return nullptr;
            }

            if(args[2] == "on")
                return new OutputCommand(line, args[0], std::nullopt, std::nullopt, std::nullopt,
                                         std::nullopt, std::nullopt, true);
            else if(args[2] == "off")
                return new OutputCommand(line, args[0], std::nullopt, std::nullopt, std::nullopt,
                                         std::nullopt, std::nullopt, false);
            else {
                logger.log(LogLevel::ERROR, "Error on line {}: invalid adaptive_sync argument",
                           line);
                return nullptr;
            }
        }

        logger.log(LogLevel::ERROR, "Error on line {}: unrecognized output subcommand '{}'", line,
                   args[1]);
        return nullptr;
    }

    bool OutputCommand::subcommand_of(CommandType type) {
        return false;
    }

    bool OutputCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::CONFIG_FIRST_LOAD && phase != ConfigLoadPhase::RELOAD)
            return true;

        std::string name = output_name.str(conf.vars);
        config::OutputConfig& config = conf.output_config[name];

        if(enabled.has_value())
            config.enabled = enabled.value();
        else if(mode.has_value())
            config.mode = mode.value();
        else if(position.has_value())
            config.pos = position.value();
        else if(transform.has_value())
            config.transform = transform.value();
        else if(scale.has_value())
            config.scale = scale.value();
        else if(adaptive_sync.has_value())
            config.adaptive_sync = adaptive_sync.value();

        return true;
    }

    BindCommand::BindCommand(int line, ParsableContent keybind, Command* command)
        : Command(line, CommandType::BIND, false),
          keybind(keybind),
          command(command) {}

    BindCommand::~BindCommand() {
        delete command;
    }

    BindCommand* BindCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() < 2) {
            if(args.size() == 0)
                logger.log(LogLevel::ERROR, "Error on line {}: missing keybind argument", line);
            else
                logger.log(LogLevel::ERROR, "Error on line {}: missing keybind command", line);
            return nullptr;
        }

        std::string keybind = args[0];
        std::string name = args[1];
        args.erase(args.begin());
        args.erase(args.begin());
        Command* command = ::parse_command(name, line, args);

        if(!command)
            return nullptr;

        if(!command->subcommand_of(CommandType::BIND)) {
            logger.log(LogLevel::ERROR,
                       "Error on line {}: this command can't be used as subcommand of 'bind'",
                       line);
            delete command;
            return nullptr;
        }

        return new BindCommand(line, ParsableContent(keybind), command);
    }

    bool BindCommand::subcommand_of(CommandType type) {
        return false;
    }

    bool BindCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::COMPOSITOR_START && phase != ConfigLoadPhase::RELOAD)
            return true;

        std::optional<config::Bind> bind = config::Bind::from_str(line, keybind.str(conf.vars));
        if(bind.has_value())
            conf.binds.push_back({ bind.value(), command });
        return true;
    }

    TerminateCommand::TerminateCommand(int line)
        : Command(line, CommandType::TERMINATE, true) {}

    TerminateCommand* TerminateCommand::parse(int line, std::vector<std::string> args) {
        if(args.size()) {
            logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
            return nullptr;
        }

        return new TerminateCommand(line);
    }

    bool TerminateCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool TerminateCommand::execute(ConfigLoadPhase phase) {
        wl_display_terminate(server.display);
        // Shouldn't even be reached
        return false;
    }

    ReloadCommand::ReloadCommand(int line)
        : Command(line, CommandType::RELOAD, true) {}

    ReloadCommand* ReloadCommand::parse(int line, std::vector<std::string> args) {
        if(args.size()) {
            logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
            return nullptr;
        }

        return new ReloadCommand(line);
    }

    bool ReloadCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool ReloadCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::BIND)
            return true;

        conf.clear();
        conf.load();
        conf.execute_phase(ConfigLoadPhase::RELOAD);

        return false;
    }

    KillCommand::KillCommand(int line)
        : Command(line, CommandType::KILL, true) {}

    KillCommand* KillCommand::parse(int line, std::vector<std::string> args) {
        if(args.size()) {
            logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
            return nullptr;
        }

        return new KillCommand(line);
    }

    bool KillCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool KillCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::BIND)
            return true;

        if(server.input_manager.seat.focused_node &&
           server.input_manager.seat.focused_node->node->type == nodes::NodeType::TOPLEVEL)
            wlr_xdg_toplevel_send_close(
                server.input_manager.seat.focused_node->node->val.toplevel->toplevel);
        else if(server.input_manager.seat.previous_toplevel &&
                server.input_manager.seat.previous_toplevel->node->val.toplevel->toplevel->base
                    ->surface->mapped)
            wlr_xdg_toplevel_send_close(
                server.input_manager.seat.previous_toplevel->node->val.toplevel->toplevel);

        return true;
    }

    WorkspaceCommand::WorkspaceCommand(int line, int id)
        : Command(line, CommandType::WORKSPACE, true),
          id(id) {}

    WorkspaceCommand* WorkspaceCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() > 1) {
            logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
            return nullptr;
        }

        if(!is_number(args[0])) {
            logger.log(LogLevel::ERROR, "Error on line {}: workspace id is not a valid number",
                       line);
            return nullptr;
        }

        return new WorkspaceCommand(line, stoi(args[0]));
    }

    bool WorkspaceCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool WorkspaceCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::BIND)
            return true;

        workspace::focus_or_create(id);

        return true;
    }

    FullscreenCommand::FullscreenCommand(int line)
        : Command(line, CommandType::FULLSCREEN, true) {}

    FullscreenCommand* FullscreenCommand::parse(int line, std::vector<std::string> args) {
        if(args.size()) {
            logger.log(LogLevel::ERROR, "Error on line {}: missing exec argument", line);
            return nullptr;
        }

        return new FullscreenCommand(line);
    }

    bool FullscreenCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool FullscreenCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::BIND)
            return true;

        if(server.input_manager.seat.focused_node &&
           server.input_manager.seat.focused_node->node->type == nodes::NodeType::TOPLEVEL)
            server.input_manager.seat.focused_node->node->val.toplevel->fullscreen();

        return true;
    }

    DebugCommand::DebugCommand(int line)
        : Command(line, CommandType::DEBUG, true) {}

    DebugCommand* DebugCommand::parse(int line, std::vector<std::string> args) {
        if(args.size()) {
            logger.log(LogLevel::ERROR, "Error on line {}: too many arguments", line);
            return nullptr;
        }

        return new DebugCommand(line);
    }

    bool DebugCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    bool DebugCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::BIND)
            return true;

        if(wlr_backend_is_wl(server.backend)) {
            wlr_wl_output_create(server.backend);
            return true;
        }
        else if(wlr_backend_is_multi(server.backend)) {
            bool done = false;
            wlr_multi_for_each_backend(
                server.backend,
                [](wlr_backend* backend, void* data) {
                    bool* done = static_cast<bool*>(data);
                    if(!*done && wlr_backend_is_wl(backend)) {
                        wlr_wl_output_create(backend);
                        *done = true;
                    }
                },
                &done);
            return true;
        }

        logger.log(LogLevel::ERROR, "No wayland backend found");

        return true;
    }
}

namespace parsing {
    Token::Token(int line, TokenType type)
        : line(line),
          type(type),
          val(std::nullopt) {}

    Token::Token(int line, TokenType type, std::string val)
        : line(line),
          type(type),
          val(val) {}

    Lexer::Lexer(const std::string& text)
        : text(text),
          index(0),
          line(1) {}

    std::vector<Token> Lexer::get_tokens() {
        std::vector<Token> tokens;

        while(1) {
            std::optional<Token> token = read_token();

            if(token.has_value()) {
                tokens.push_back(token.value());
                if(token.value().type == TokenType::NEW_LINE)
                    line++;
            }

            if(peek() == '\0')
                break;
        }

        return tokens;
    }

    char Lexer::consume() {
        if(index >= text.size())
            return '\0';
        return text[index++];
    }

    char Lexer::peek() {
        if(index >= text.size())
            return '\0';
        return text[index];
    }

    std::optional<Token> Lexer::read_token() {
        while(peek() == ' ') consume();
        char c = peek();

        if(c == '\n') {
            consume();
            return Token(line, TokenType::NEW_LINE);
        }
        else if(c == '\0') {
            consume();
            return Token(line, TokenType::FILE_END);
        }
        else if(c == '{') {
            consume();
            return Token(line, TokenType::BRACKET_OPEN);
        }
        else if(c == '}') {
            consume();
            return Token(line, TokenType::BRACKET_CLOSE);
        }
        else if(c == '#') {
            while(peek() != '\n' && peek() != '\0') consume();
            return std::nullopt;
        }
        else if(c == '"') {
            std::optional<std::string> s = read_string();
            if(s.has_value())
                return Token(line, TokenType::STRING, s.value());
            return Token(line, TokenType::UNKNOWN);
        }
        else
            return Token(line, TokenType::ARG, read_word());
    }

    std::string Lexer::read_word() {
        std::string result;
        while(1) {
            char c = peek();
            if(c == '#' || c == '{') {
                while(peek() != '\n' && peek() != '\0') consume();
                break;
            }

            while(peek() == ' ') consume();
            if(c == '\0' || c == '\n' || c == ' ')
                break;

            consume();
            result += c;
        }

        return result;
    }

    std::optional<std::string> Lexer::read_string() {
        std::string result;
        consume();

        while(1) {
            char c = peek();
            if(c == '"')
                break;

            if(c == '\0' || c == '\n') {
                logger.log(LogLevel::ERROR, "Error on line {}: opened string is never closed",
                           line);
                return std::nullopt;
            }

            consume();
            result += c;
        }

        consume();
        return result;
    }

    Parser::Parser(const std::string& text)
        : index(0) {
        Lexer lexer(text);
        tokens = lexer.get_tokens();
    }

    std::vector<commands::Command*> Parser::parse() {
        std::vector<commands::Command*> parsed;

        while(1) {
            Token token = peek();
            while((token = peek()).type == TokenType::NEW_LINE) {
                token = consume();
            }

            if(token.type == TokenType::FILE_END)
                break;

            std::vector<commands::Command*> commands = read_commands();

            for(auto& command : commands) {
                if(command)
                    parsed.push_back(command);
            }
        }

        return parsed;
    }

    Token Parser::consume() {
        if(index >= tokens.size())
            return Token(0, TokenType::FILE_END);
        return tokens[index++];
    }

    Token Parser::peek() {
        if(index >= tokens.size())
            return Token(0, TokenType::FILE_END);
        return tokens[index];
    }

    std::vector<commands::Command*> Parser::read_commands() {
        std::vector<std::string> args;
        Token token = consume();

        if(token.type != TokenType::ARG) {
            logger.log(LogLevel::ERROR, "Error on line {}: expected command", token.line);
            return {};
        }

        int line = token.line;
        std::string name = token.val.value();

        while(1) {
            Token token = peek();

            if(token.type == TokenType::NEW_LINE || token.type == TokenType::BRACKET_CLOSE ||
               token.type == TokenType::FILE_END)
                break;
            else if(token.type == TokenType::BRACKET_OPEN) {
                std::vector<std::vector<std::string>> block = read_block();
                token = consume();
                if(token.type != TokenType::BRACKET_CLOSE) {
                    logger.log(LogLevel::ERROR, "Error on line {}: expected '}}'", token.line);
                    return {};
                }
                return ::parse(line, name, args, block);
            }
            else if(token.type == TokenType::STRING || token.type == TokenType::ARG) {
                consume();
                args.push_back(token.val.value());
            }
        }

        return ::parse(line, name, args, std::nullopt);
    }

    std::vector<std::vector<std::string>> Parser::read_block() {
        // Consume the opening bracket
        Token token = consume();
        if(token.type != TokenType::BRACKET_OPEN) {
            logger.log(LogLevel::ERROR, "Error on line {}: expected '{{'", token.line);
            return {};
        }

        std::vector<std::vector<std::string>> block;

        while(1) {
            while(peek().type == TokenType::NEW_LINE) {
                consume();
            }

            if(peek().type == TokenType::BRACKET_CLOSE || peek().type == TokenType::FILE_END) {
                break;
            }

            std::vector<std::string> line_args;
            while(1) {
                token = peek();

                if(token.type == TokenType::NEW_LINE || token.type == TokenType::BRACKET_CLOSE ||
                   token.type == TokenType::FILE_END) {
                    break;
                }
                else if(token.type == TokenType::STRING || token.type == TokenType::ARG) {
                    consume();
                    line_args.push_back(token.val.value());
                }
                else {
                    logger.log(LogLevel::ERROR, "Error on line {}: unexpected token in block",
                               token.line);
                    consume();
                }
            }

            if(!line_args.empty()) {
                block.push_back(line_args);
            }

            if(peek().type == TokenType::NEW_LINE) {
                consume();
            }
        }

        return block;
    }
}
