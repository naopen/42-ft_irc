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

    // メッセージの最大長をチェック（過剰に長いメッセージを防ぐ）
    if (msg.length() > 512) {
        std::cout << "\033[1;31m[PARSER] Message too long, truncating to 512 characters\033[0m" << std::endl;
        msg = msg.substr(0, 512);
    }

    // 不正な制御文字をチェック
    for (size_t i = 0; i < msg.length(); ++i) {
        // NUL, CR, LF以外の制御文字をフィルタリング
        if (msg[i] < 32 && msg[i] != '\r' && msg[i] != '\n') {
            std::cout << "\033[1;31m[PARSER] Invalid control character at position " << i << "\033[0m" << std::endl;
            _valid = false;
            return;
        }
    }

    // プレフィックスの抽出（オプション）
    if (msg[0] == ':') {
        size_t spacePos = msg.find(' ');
        if (spacePos == std::string::npos) {
            _valid = false;
            return;
        }

        _prefix = msg.substr(1, spacePos - 1);

        // プレフィックスの形式チェック（空でないことを確認）
        if (_prefix.empty()) {
            std::cout << "\033[1;31m[PARSER] Empty prefix\033[0m" << std::endl;
            _valid = false;
            return;
        }

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

        // コマンド名のバリデーション
        if (_command.empty() || _command.length() > 16) {
            std::cout << "\033[1;31m[PARSER] Invalid command name: '" << _command << "'\033[0m" << std::endl;
            _valid = false;
            return;
        }

        _valid = true;
        return;
    }

    _command = Utils::toUpper(msg.substr(0, spacePos));

    // コマンド名のバリデーション
    if (_command.empty() || _command.length() > 16) {
        std::cout << "\033[1;31m[PARSER] Invalid command name: '" << _command << "'\033[0m" << std::endl;
        _valid = false;
        return;
    }

    msg = msg.substr(spacePos + 1);

    // 空白削除
    while (!msg.empty() && msg[0] == ' ') {
        msg = msg.substr(1);
    }

    // パラメータの抽出
    size_t paramCount = 0;
    while (!msg.empty() && paramCount < 15) { // 最大15個のパラメータに制限
        // 最後のパラメータが':'で始まる場合（残りのすべてを1つのパラメータとして処理）
        if (msg[0] == ':') {
            _params.push_back(msg.substr(1));
            paramCount++;
            break;
        }

        // 通常のパラメータ
        spacePos = msg.find(' ');
        if (spacePos == std::string::npos) {
            _params.push_back(msg);
            paramCount++;
            break;
        }

        std::string param = msg.substr(0, spacePos);
        if (!param.empty()) {
            _params.push_back(param);
            paramCount++;
        }

        msg = msg.substr(spacePos + 1);

        // 空白削除
        while (!msg.empty() && msg[0] == ' ') {
            msg = msg.substr(1);
        }
    }

    // 過剰なパラメータ数のチェック
    if (!msg.empty() && paramCount >= 15) {
        std::cout << "\033[1;31m[PARSER] Too many parameters (max 15), truncating\033[0m" << std::endl;
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
