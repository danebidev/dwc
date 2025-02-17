#include "config/commands.hpp"

#include <unistd.h>

#include <cassert>

#include "config/config.hpp"
#include "server.hpp"
#include "wlr.hpp"

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
                    result += envs[currentVar];
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

    Command::Command(int line, CommandType type, bool can_have_block)
        : line(line),
          type(type),
          can_have_block(can_have_block) {}

    Command* Command::parse(int line, std::string name, std::vector<std::string> args,
                            std::optional<std::vector<Command*>> block) {
        Command* command;

        if(name == "set")
            command = SetCommand::parse(line, args);
        else if(name == "env")
            command = EnvCommand::parse(line, args);
        else if(name == "exec")
            command = ExecCommand::parse(line, args);
        else if(name == "exec_always")
            command = ExecAlwaysCommand::parse(line, args);
        else if(name == "output")
            command = OutputCommand::parse(line, args, block);
        else if(name == "bind")
            command = BindCommand::parse(line, args);
        else if(name == "terminate")
            command = TerminateCommand::parse(line, args);
        else {
            wlr_log(WLR_ERROR, "Error on line %d: command '%s' not recognized", line, name.c_str());
            return nullptr;
        }

        if(!command->can_have_block && block.has_value()) {
            wlr_log(WLR_ERROR,
                    "Error on line %d: bracket block passed to command that doesn't require it",
                    line);
            return nullptr;
        }

        return command;
    }

    SetCommand::SetCommand(int line, std::string name, ParsableContent content)
        : Command(line, CommandType::SET, false),
          name(name),
          content(content) {}

    SetCommand* SetCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            wlr_log(WLR_ERROR, "Error on line %d: missing set argument", line);
            return nullptr;
        }
        std::string value;
        for(size_t i = 1; i < args.size(); i++) {
            value += args[i] + (i == args.size() - 1 ? "" : " ");
        }

        return new SetCommand(line, args[1], ParsableContent(value));
    }

    bool SetCommand::subcommand_of(CommandType type) {
        return false;
    }

    void SetCommand::execute(ConfigLoadPhase phase) {
        // Only set vars on config first load and reloads
        if(phase == ConfigLoadPhase::COMPOSITOR_START)
            return;
        conf.vars[name] = content.str(conf.vars);
    }

    EnvCommand::EnvCommand(int line, std::string name, ParsableContent content)
        : Command(line, CommandType::ENV, false),
          name(name),
          content(content) {}

    EnvCommand* EnvCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            wlr_log(WLR_ERROR, "Error on line %d: missing env argument", line);
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

    void EnvCommand::execute(ConfigLoadPhase phase) {
        // Only set envs once, they don't get reloaded
        if(phase != ConfigLoadPhase::CONFIG_FIRST_LOAD)
            return;
        setenv(name.c_str(), content.str(conf.vars).c_str(), true);
        conf.vars[name] = content.str(conf.vars);
    }

    ExecCommand::ExecCommand(int line, ParsableContent content)
        : Command(line, CommandType::EXEC, false),
          content(content) {}

    ExecCommand* ExecCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            wlr_log(WLR_ERROR, "Error on line %d: missing exec argument", line);
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

    void ExecCommand::execute(ConfigLoadPhase phase) {
        // execs only get executed on first start
        if(phase != ConfigLoadPhase::COMPOSITOR_START && phase != ConfigLoadPhase::BIND)
            return;
        int pid = fork();
        if(pid < 0) {
            wlr_log(WLR_ERROR, "fork() failed - exiting to avoid errors");
            exit(EXIT_FAILURE);
        }

        if(pid == 0)
            execl("/bin/sh", "/bin/sh", "-c", content.str(conf.vars).c_str(), (void*)nullptr);
    }

    ExecAlwaysCommand::ExecAlwaysCommand(int line, ParsableContent content)
        : Command(line, CommandType::EXEC_ALWAYS, false),
          content(content) {}

    ExecAlwaysCommand* ExecAlwaysCommand::parse(int line, std::vector<std::string> args) {
        if(args.size() == 0) {
            wlr_log(WLR_ERROR, "Error on line %d: missing exec_always argument", line);
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

    void ExecAlwaysCommand::execute(ConfigLoadPhase phase) {
        if(phase == ConfigLoadPhase::CONFIG_FIRST_LOAD)
            return;

        int pid = fork();
        if(pid < 0) {
            wlr_log(WLR_ERROR, "fork() failed - exiting to avoid errors");
            exit(EXIT_FAILURE);
        }

        if(pid == 0)
            execl("/bin/sh", "/bin/sh", "-c", content.str(conf.vars).c_str(), (void*)nullptr);
    }

    OutputCommand::OutputCommand(int line, ParsableContent output_name, std::string mode,
                                 std::string position, bool adaptive_sync)
        : Command(line, CommandType::OUTPUT, false),
          output_name(output_name),
          mode(mode),
          position(position),
          adaptive_sync(adaptive_sync) {}

    OutputCommand::~OutputCommand() {}

    OutputCommand* OutputCommand::parse(int line, std::vector<std::string> args,
                                        std::optional<std::vector<commands::Command*>> block) {
        if(args.size() == 0) {
            wlr_log(WLR_ERROR, "Error on line %d: missing output name", line);
            return nullptr;
        }
        // TODO
        return nullptr;
    }

    bool OutputCommand::subcommand_of(CommandType type) {
        return false;
    }

    void OutputCommand::execute(ConfigLoadPhase phase) {
        // TODO
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
                wlr_log(WLR_ERROR, "Error on line %d: missing keybind argument", line);
            else
                wlr_log(WLR_ERROR, "Error on line %d: missing keybind command", line);
            return nullptr;
        }

        std::string keybind = args[0];
        std::string name = args[1];
        args.erase(args.begin());
        args.erase(args.begin());
        Command* command = Command::parse(line, name, args, std::nullopt);

        return new BindCommand(line, ParsableContent(keybind), command);
    }

    bool BindCommand::subcommand_of(CommandType type) {
        return false;
    }

    void BindCommand::execute(ConfigLoadPhase phase) {
        if(phase != ConfigLoadPhase::COMPOSITOR_START && phase != ConfigLoadPhase::RELOAD)
            return;

        config::Bind* bind = config::Bind::from_str(line, keybind.str(conf.vars));
        if(bind)
            conf.binds.push_back({ bind, command });
    }

    TerminateCommand::TerminateCommand(int line)
        : Command(line, CommandType::TERMINATE, false) {}

    TerminateCommand* TerminateCommand::parse(int line, std::vector<std::string> args) {
        if(args.size()) {
            wlr_log(WLR_ERROR, "Error on line %d: too many arguments", line);
        }

        return new TerminateCommand(line);
    }

    bool TerminateCommand::subcommand_of(CommandType type) {
        return type == CommandType::BIND;
    }

    void TerminateCommand::execute(ConfigLoadPhase phase) {
        wl_display_terminate(server.display);
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
            if(c == '#') {
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
                wlr_log(WLR_ERROR, "Error on line %d: opened string is never closed", line);
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

    std::vector<commands::Command*> Parser::parse(bool block) {
        std::vector<commands::Command*> commands;

        while(1) {
            Token token = peek();
            while((token = peek()).type == TokenType::NEW_LINE) {
                token = consume();
            }

            if(token.type == TokenType::FILE_END)
                break;
            if(block && token.type == TokenType::BRACKET_CLOSE)
                break;

            commands::Command* command = read_command();

            if(command)
                commands.push_back(command);
        }

        return commands;
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

    commands::Command* Parser::read_command() {
        std::vector<std::string> args;
        Token token = consume();

        if(token.type != TokenType::ARG) {
            wlr_log(WLR_ERROR, "Error on line %d: expected command", token.line);
            return nullptr;
        }

        int line = token.line;
        std::string name = token.val.value();

        while(1) {
            Token token = peek();

            if(token.type == TokenType::NEW_LINE || token.type == TokenType::BRACKET_CLOSE ||
               token.type == TokenType::FILE_END)
                break;
            else if(token.type == TokenType::BRACKET_OPEN) {
                std::vector<commands::Command*> block = parse(true);
                token = consume();
                if(token.type != TokenType::BRACKET_CLOSE) {
                    wlr_log(WLR_ERROR, "Error on line %d: expected '}'", token.line);
                    return nullptr;
                }
                return commands::Command::parse(line, name, args, block);
            }
            else if(token.type == TokenType::STRING || token.type == TokenType::ARG) {
                consume();
                args.push_back(token.val.value());
            }
        }

        return commands::Command::parse(line, name, args, std::nullopt);
    }
}
