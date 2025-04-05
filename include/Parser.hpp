#ifndef PARSER_HPP
# define PARSER_HPP

# include "Utils.hpp"

class Parser {
private:
    std::string _message;
    std::string _prefix;
    std::string _command;
    std::vector<std::string> _params;
    bool _valid;

public:
    Parser(const std::string& message);
    ~Parser();

    // パース処理
    void parse();

    // ゲッター
    std::string getPrefix() const;
    std::string getCommand() const;
    std::vector<std::string> getParams() const;
    bool isValid() const;

    // デバッグ用
    void printParsedMessage() const;
};

#endif
