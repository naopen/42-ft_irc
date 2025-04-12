#include "../include/Client.hpp"

Client::Client(int fd, const std::string& hostname)
    : _fd(fd), _hostname(hostname), _status(CONNECTING), _passAccepted(false),
      _operator(false), _away(false) {
    _lastActivity = time(NULL);
}

Client::~Client() {
    // ソケットを閉じる
    if (_fd >= 0) {
        close(_fd);
        _fd = -1;
    }
}

// ゲッター
int Client::getFd() const {
    return _fd;
}

std::string Client::getNickname() const {
    return _nickname;
}

std::string Client::getUsername() const {
    return _username;
}

std::string Client::getHostname() const {
    return _hostname;
}

std::string Client::getRealname() const {
    return _realname;
}

ClientStatus Client::getStatus() const {
    return _status;
}

bool Client::isPassAccepted() const {
    return _passAccepted;
}

bool Client::isOperator() const {
    return _operator;
}

time_t Client::getLastActivity() const {
    return _lastActivity;
}

bool Client::isAway() const {
    return _away;
}

std::string Client::getAwayMessage() const {
    return _awayMessage;
}

std::vector<std::string> Client::getChannels() const {
    return _channels;
}

std::string Client::getPrefix() const {
    // nickname!username@hostname 形式のプレフィックスを返す
    return _nickname + "!" + _username + "@" + _hostname;
}

// セッター
void Client::setNickname(const std::string& nickname) {
    _nickname = nickname;
}

void Client::setUsername(const std::string& username) {
    _username = username;
}

void Client::setRealname(const std::string& realname) {
    _realname = realname;
}

void Client::setStatus(ClientStatus status) {
    _status = status;

    // ステータス変更をログに出力
    std::string statusStr;
    switch (status) {
        case CONNECTING:
            statusStr = "CONNECTING";
            break;
        case REGISTERING:
            statusStr = "REGISTERING";
            break;
        case REGISTERED:
            statusStr = "REGISTERED";
            break;
        default:
            statusStr = "UNKNOWN";
    }

    std::cout << "\033[1;35m[STATUS] Client " << _fd;
    if (!_nickname.empty()) {
        std::cout << " (" << _nickname << ")";
    }
    std::cout << " status changed to " << statusStr << "\033[0m" << std::endl;
}

void Client::setPassAccepted(bool accepted) {
    bool oldValue = _passAccepted;
    _passAccepted = accepted;

    // 値が変わった場合だけログを出力
    if (oldValue != accepted) {
        std::cout << "\033[1;35m[AUTH] Client " << _fd;
        if (!_nickname.empty()) {
            std::cout << " (" << _nickname << ")";
        }
        std::cout << " password " << (accepted ? "accepted" : "rejected") << "\033[0m" << std::endl;
    }
}

void Client::setOperator(bool op) {
    bool oldValue = _operator;
    _operator = op;

    // 値が変わった場合だけログを出力
    if (oldValue != op) {
        std::cout << "\033[1;35m[OPER] Client " << _fd;
        if (!_nickname.empty()) {
            std::cout << " (" << _nickname << ")";
        }
        std::cout << " is " << (op ? "now" : "no longer") << " an operator\033[0m" << std::endl;
    }
}

void Client::updateLastActivity() {
    _lastActivity = time(NULL);
}

void Client::setAway(bool away, const std::string& message) {
    bool oldValue = _away;
    _away = away;
    _awayMessage = message;

    // 値が変わった場合だけログを出力
    if (oldValue != away) {
        std::cout << "\033[1;35m[AWAY] Client " << _fd;
        if (!_nickname.empty()) {
            std::cout << " (" << _nickname << ")";
        }
        std::cout << " is " << (away ? "now away" : "no longer away");
        if (away && !message.empty()) {
            std::cout << " (" << message << ")";
        }
        std::cout << "\033[0m" << std::endl;
    }
}

// チャンネル管理
void Client::addChannel(const std::string& channel) {
    if (!isInChannel(channel)) {
        _channels.push_back(channel);

        std::cout << "\033[1;33m[CHANNEL] Client " << _fd;
        if (!_nickname.empty()) {
            std::cout << " (" << _nickname << ")";
        }
        std::cout << " joined channel " << channel << "\033[0m" << std::endl;
    }
}

void Client::removeChannel(const std::string& channel) {
    std::vector<std::string>::iterator it = std::find(_channels.begin(), _channels.end(), channel);
    if (it != _channels.end()) {
        _channels.erase(it);

        std::cout << "\033[1;33m[CHANNEL] Client " << _fd;
        if (!_nickname.empty()) {
            std::cout << " (" << _nickname << ")";
        }
        std::cout << " left channel " << channel << "\033[0m" << std::endl;
    }
}

bool Client::isInChannel(const std::string& channel) const {
    return std::find(_channels.begin(), _channels.end(), channel) != _channels.end();
}

// バッファ操作
void Client::appendToBuffer(const std::string& data) {
    // エスケープシーケンスを検出してフィルタリング
    std::string filtered;
    for (size_t i = 0; i < data.length(); i++) {
        // エスケープシーケンス（矢印キーなど）の開始
        if (data[i] == '\033') {
            // 矢印キーなどのエスケープシーケンスをスキップ
            // 典型的なエスケープシーケンスは \033[A (上矢印) のような形式
            while (i < data.length() &&
                  !(data[i] >= 'A' && data[i] <= 'Z') &&
                  !(data[i] >= 'a' && data[i] <= 'z')) {
                i++;
            }
            // 終端文字もスキップ
            if (i < data.length()) {
                i++;
            }
            continue;
        }

        // 通常の文字なら追加
        if (data[i] != '\0') { // NULL文字をフィルタリング
            filtered += data[i];
        }
    }

    _buffer += filtered;
    updateLastActivity();
}

std::string Client::getBuffer() const {
    return _buffer;
}

void Client::clearBuffer() {
    _buffer.clear();
}

std::vector<std::string> Client::getCompleteMessages() {
    std::vector<std::string> messages;
    size_t pos = 0;
    size_t found;

    // \r\n または \n のいずれかで区切られた完全なメッセージを検索
    while (true) {
        // まず\r\nを探す
        found = _buffer.find("\r\n", pos);
        if (found != std::string::npos) {
            std::string message = _buffer.substr(pos, found - pos);
            messages.push_back(message);
            pos = found + 2; // \r\nの長さ分進める
            continue;
        }

        // \r\nがなければ\nを探す
        found = _buffer.find("\n", pos);
        if (found != std::string::npos) {
            std::string message = _buffer.substr(pos, found - pos);
            // \rを除去（\r\nの代わりに\nだけが使われる場合の対策）
            if (!message.empty() && message[message.length() - 1] == '\r') {
                message = message.substr(0, message.length() - 1);
            }
            messages.push_back(message);
            pos = found + 1; // \nの長さ分進める
            continue;
        }

        // どちらも見つからなければループを抜ける
        break;
    }

    // 処理済みの部分をバッファから削除
    if (pos > 0) {
        _buffer = _buffer.substr(pos);
    }

    return messages;
}

// メッセージ送信
void Client::sendMessage(const std::string& message) {
    if (_fd >= 0) {
        std::string fullMessage = message;
        if (fullMessage.find("\r\n") == std::string::npos) {
            fullMessage += "\r\n";
        }

        std::cout << "\033[1;34m[SEND] To fd " << _fd;
        if (!_nickname.empty()) {
            std::cout << " (" << _nickname << ")";
        }
        std::cout << ": " << fullMessage << "\033[0m";

        ssize_t sent = send(_fd, fullMessage.c_str(), fullMessage.length(), 0);
        if (sent < 0) {
            std::cerr << "\033[1;31m[ERROR] Error sending message to client: " << strerror(errno) << "\033[0m" << std::endl;
        } else if (static_cast<size_t>(sent) != fullMessage.length()) {
            std::cerr << "\033[1;33m[WARNING] Not all data was sent to client\033[0m" << std::endl;
        }
    } else {
        std::cerr << "\033[1;31m[ERROR] Attempting to send message to invalid fd: " << _fd << "\033[0m" << std::endl;
    }
}

void Client::sendNumericReply(int code, const std::string& message) {
    std::string target = _nickname.empty() ? "*" : _nickname;
    std::string formattedReply = Utils::formatResponse(code, target, message);
    sendMessage(formattedReply);
}

// ユーザー認証のための関数
bool Client::isRegistered() const {
    return _status == REGISTERED;
}

bool Client::hasCompletedRegistration() const {
    return _passAccepted && !_nickname.empty() && !_username.empty();
}
