#include "../include/Parser.hpp"

Parser::Parser(const std::string& message)
    : _message(message), _valid(false)
{
    parse();
}

Parser::~Parser() {
    // 特に何もしない
}

void Parser::parse() {
    std::string msg = _message;

    // 空メッセージのチェック
    if (msg.empty()) {
        _valid = false;
        return;
    }

    // プレフィックスの抽出（オプション）
    if (msg[0] == ':') {
        size_t spacePos = msg.find(' ');
        if (spacePos == std::string::npos) {
            _valid = false;
            return;
        }

        _prefix = msg.substr(1, spacePos - 1);
        msg = msg.substr(spacePos + 1);

        // 空白削除
        while (!msg.empty() && msg[0] == ' ') {
            msg = msg.substr(1);
        }
    }

    // コマンドの抽出
    size_t spacePos = msg.find(' ');
    if (spacePos == std::string::npos) {
        _command = Utils::toUpper(msg);
        _valid = true;
        return;
    }

    _command = Utils::toUpper(msg.substr(0, spacePos));
    msg = msg.substr(spacePos + 1);

    // 空白削除
    while (!msg.empty() && msg[0] == ' ') {
        msg = msg.substr(1);
    }

    // パラメータの抽出
    while (!msg.empty()) {
        // 最後のパラメータが':'で始まる場合（残りのすべてを1つのパラメータとして処理）
        if (msg[0] == ':') {
            _params.push_back(msg.substr(1));
            break;
        }

        // 通常のパラメータ
        spacePos = msg.find(' ');
        if (spacePos == std::string::npos) {
            _params.push_back(msg);
            break;
        }

        _params.push_back(msg.substr(0, spacePos));
        msg = msg.substr(spacePos + 1);

        // 空白削除
        while (!msg.empty() && msg[0] == ' ') {
            msg = msg.substr(1);
        }
    }

    _valid = true;
}

std::string Parser::getPrefix() const {
    return _prefix;
}

std::string Parser::getCommand() const {
    return _command;
}

std::vector<std::string> Parser::getParams() const {
    return _params;
}

bool Parser::isValid() const {
    return _valid;
}

void Parser::printParsedMessage() const {
    std::cout << "Message: " << _message << std::endl;
    std::cout << "Prefix: " << _prefix << std::endl;
    std::cout << "Command: " << _command << std::endl;
    std::cout << "Params: ";
    for (std::vector<std::string>::const_iterator it = _params.begin(); it != _params.end(); ++it) {
        std::cout << "[" << *it << "] ";
    }
    std::cout << std::endl;
    std::cout << "Valid: " << (_valid ? "true" : "false") << std::endl;
}
