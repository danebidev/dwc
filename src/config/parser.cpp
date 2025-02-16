#include "config/parser.hpp"

#include <cassert>

#include "wlr.hpp"

namespace statements {
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

    Statement::Statement(StatementType type)
        : type(type) {}

    SetStatement::SetStatement(std::string name, ParsableContent content)
        : Statement(StatementType::SET),
          name(name),
          content(content) {}

    EnvStatement::EnvStatement(std::string name, ParsableContent content)
        : Statement(StatementType::ENV),
          name(name),
          content(content) {}

    ExecStatement::ExecStatement(ParsableContent content)
        : Statement(StatementType::EXEC),
          content(content) {}

    ExecAlwaysStatement::ExecAlwaysStatement(ParsableContent content)
        : Statement(StatementType::EXEC_ALWAYS),
          content(content) {}

    OutputStatement::OutputStatement(ParsableContent output_name, std::string mode,
                                     std::string position, bool adaptive_sync)
        : Statement(StatementType::OUTPUT),
          output_name(output_name),
          mode(mode),
          position(position),
          adaptive_sync(adaptive_sync) {}
}

namespace parser {
    Parser::Parser(const std::string& text)
        : text(text),
          index(0),
          line(1) {}

    std::vector<statements::Statement*> Parser::parse() {
        std::vector<statements::Statement*> statements;

        while(1) {
            std::optional<statements::Statement*> statement = read_statement();
            assert(peek() == '\n' || peek() == '\0');

            if(statement.has_value())
                statements.push_back(statement.value());

            if(consume() == '\0')
                break;

            line++;
        }

        return statements;
    }

    std::optional<statements::Statement*> Parser::read_statement() {
        std::string keyword = read_word();
        if(keyword.empty())
            return std::nullopt;

        if(keyword == "set" || keyword == "env")
            return read_set(keyword);
        else if(keyword == "exec" || keyword == "exec_always")
            return read_exec(keyword);
        else if(keyword == "output")
            return read_output();

        wlr_log(WLR_ERROR, "config (line %d): unrecognized command '%s'", line, keyword.c_str());
        return std::nullopt;
    }

    std::string Parser::read_word() {
        std::string result;
        while(1) {
            char c = peek();
            while(peek() == ' ') consume();
            if(c == '\0' || c == '\n' || c == ' ')
                break;

            consume();
            result += c;
        }
        return result;
    }

    std::optional<std::string> Parser::read_string() {
        if(peek() != '"')
            return read_word();
        consume();

        std::string result;

        while(1) {
            char c = peek();
            if(c == '"')
                break;

            if(c == '\0' || c == '\n') {
                wlr_log(WLR_ERROR, "config (line %d): opened string is never closed", line);
                return std::nullopt;
            }

            consume();
            result += c;
        }
        consume();
        while(peek() == ' ') consume();
        return result;
    }

    std::optional<statements::Statement*> Parser::read_set(std::string keyword) {
        std::string name = read_word();

        std::optional<std::string> value = read_string();
        if(!value.has_value()) {
            wlr_log(WLR_ERROR, "config (line %d): failed reading '%s' command", line,
                    keyword.c_str());
            return std::nullopt;
        }
        if(peek() != '\n' && peek() != '\0') {
            wlr_log(WLR_ERROR, "config (line %d): too many arguments to %s command", line,
                    keyword.c_str());
            return std::nullopt;
        }

        if(keyword == "set")
            return new statements::SetStatement(name, statements::ParsableContent(value.value()));
        else
            return new statements::EnvStatement(name, statements::ParsableContent(value.value()));
    }

    std::optional<statements::Statement*> Parser::read_exec(std::string keyword) {
        std::optional<std::string> command = read_string();
        if(!command.has_value()) {
            wlr_log(WLR_ERROR, "config (line %d): failed reading '%s' command", line,
                    keyword.c_str());
            return std::nullopt;
        }
        if(peek() != '\n' && peek() != '\0') {
            wlr_log(WLR_ERROR, "config (line %d): too many arguments to %s command", line,
                    keyword.c_str());
            return std::nullopt;
        }

        if(keyword == "exec")
            return new statements::ExecStatement(statements::ParsableContent(command.value()));
        else
            return new statements::ExecAlwaysStatement(
                statements::ParsableContent(command.value()));
    }

    std::optional<statements::Statement*> Parser::read_output() {
        // TODO
        return std::nullopt;
    }

    char Parser::consume() {
        if(index + 1 >= text.size())
            return '\0';
        return text[index++];
    }

    char Parser::peek(int n) {
        if(index + n - 1 >= text.size())
            return '\0';
        return text[index + n - 1];
    }
}
