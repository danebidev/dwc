#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "util.hpp"

namespace commands {
    enum class CommandType { SET, ENV, EXEC, EXEC_ALWAYS, OUTPUT, BIND };

    struct Command;

    // A string that may contain variables that have to be substituted
    // Variable are set with the 'set' command, and are then used with $myVar
    // The variable name continues until a non-alphnumeric character is found
    // Variables that are not set will be replaced with an empty string
    class ParsableContent {
        public:
        std::string str(std::unordered_map<std::string, std::string>& envs);

        ParsableContent(std::string content);

        private:
        std::string content;
    };

    struct Command {
        int line;
        CommandType type;
        bool can_have_block;

        Command(int line, CommandType type, bool can_have_block);
        virtual ~Command() = default;

        static Command* parse(int line, std::string name, std::vector<std::string> args,
                              std::optional<std::vector<Command*>> block);
        virtual bool subcommand_of(CommandType type) = 0;
        virtual void execute(ConfigLoadPhase phase) = 0;
    };

    struct SetCommand : Command {
        std::string name;
        ParsableContent content;

        SetCommand(int line, std::string name, ParsableContent content);

        static SetCommand* parse(int line, std::vector<std::string> args);
        bool subcommand_of(CommandType type) override;
        void execute(ConfigLoadPhase phase) override;
    };

    struct EnvCommand : Command {
        std::string name;
        ParsableContent content;

        EnvCommand(int line, std::string name, ParsableContent content);

        static EnvCommand* parse(int line, std::vector<std::string> args);
        bool subcommand_of(CommandType type) override;
        void execute(ConfigLoadPhase phase) override;
    };

    struct ExecCommand : Command {
        ParsableContent content;

        ExecCommand(int line, ParsableContent content);

        static ExecCommand* parse(int line, std::vector<std::string> args);
        bool subcommand_of(CommandType type) override;
        void execute(ConfigLoadPhase phase) override;
    };

    struct ExecAlwaysCommand : Command {
        ParsableContent content;

        ExecAlwaysCommand(int line, ParsableContent content);

        static ExecAlwaysCommand* parse(int line, std::vector<std::string> args);
        bool subcommand_of(CommandType type) override;
        void execute(ConfigLoadPhase phase) override;
    };

    struct OutputCommand : Command {
        // Can be '*', to represent all outputs
        // More complex globbing like 'DP-*' is not supported
        ParsableContent output_name;

        std::string mode;
        std::string position;
        bool adaptive_sync;

        OutputCommand(int line, ParsableContent output_name, std::string mode, std::string position,
                      bool adaptive_sync);
        ~OutputCommand() override;

        static OutputCommand* parse(int line, std::vector<std::string> args,
                                    std::optional<std::vector<Command*>> block);
        bool subcommand_of(CommandType type) override;
        void execute(ConfigLoadPhase phase) override;
    };

    struct BindCommand : Command {
        ParsableContent keybind;
        Command* command;

        BindCommand(int line, ParsableContent keybind, Command* command);
        ~BindCommand() override;

        static BindCommand* parse(int line, std::vector<std::string> args);
        bool subcommand_of(CommandType type) override;
        void execute(ConfigLoadPhase phase) override;
    };
}

namespace parsing {
    enum class TokenType { UNKNOWN, FILE_END, NEW_LINE, ARG, STRING, BRACKET_OPEN, BRACKET_CLOSE };

    // A whole lexer and parser might be overkill for this, but who cares
    struct Token {
        int line;
        TokenType type;
        std::optional<std::string> val;

        Token(int line, TokenType type);
        Token(int line, TokenType type, std::string val);
    };

    class Lexer {
        public:
        Lexer(const std::string& text);
        std::vector<Token> get_tokens();

        private:
        std::string text;
        size_t index;
        int line;

        char consume();
        char peek();

        Token read_token();
        std::string read_word();
        std::optional<std::string> read_string();
    };

    class Parser {
        public:
        Parser(const std::string& text);

        std::vector<commands::Command*> parse(bool block = false);

        private:
        std::vector<Token> tokens;
        size_t index;

        Token consume();
        Token peek();

        commands::Command* read_command();
    };
}
