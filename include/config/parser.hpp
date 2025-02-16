#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace statements {
    enum class StatementType { SET, ENV, EXEC, EXEC_ALWAYS, OUTPUT };

    // Some string, that may contain variables that have to be parsed
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

    struct Statement {
        StatementType type;

        Statement(StatementType type);
    };

    struct SetStatement : Statement {
        std::string name;
        ParsableContent content;

        SetStatement(std::string name, ParsableContent content);
    };

    struct EnvStatement : Statement {
        std::string name;
        ParsableContent content;

        EnvStatement(std::string name, ParsableContent content);
    };

    struct ExecStatement : Statement {
        ParsableContent content;

        ExecStatement(ParsableContent content);
    };

    struct ExecAlwaysStatement : Statement {
        ParsableContent content;

        ExecAlwaysStatement(ParsableContent content);
    };

    struct OutputStatement : Statement {
        // Can be '*', to represent all outputs
        // More complex globbing like 'DP-*' is not supported
        ParsableContent output_name;

        std::string mode;
        std::string position;
        bool adaptive_sync;

        OutputStatement(ParsableContent output_name, std::string mode, std::string position,
                        bool adaptive_sync);
    };
}

namespace parser {
    class Parser {
        public:
        Parser(const std::string& text);

        std::vector<statements::Statement*> parse();

        private:
        const std::string& text;
        size_t index;
        int line;

        char consume();
        char peek(int n = 1);

        std::optional<statements::Statement*> read_statement();
        std::string read_word();
        std::optional<std::string> read_string();

        std::optional<statements::Statement*> read_set(std::string keyword);
        std::optional<statements::Statement*> read_exec(std::string keyword);
        std::optional<statements::Statement*> read_output();
    };
}
