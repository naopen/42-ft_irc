#include "../include/Channel.hpp"

Channel::Channel(const std::string& name, Client* creator)
    : _name(name), _inviteOnly(false), _topicRestricted(true), _userLimit(0),
      _hasUserLimit(false), _creationTime(time(NULL))
{
    if (creator) {
        _clients.push_back(creator);
        _operators.push_back(creator->getNickname());

        std::cout << "\033[1;33m[CHANNEL] Created " << name << " with creator "
                  << creator->getNickname() << " as operator\033[0m" << std::endl;
    }
}

Channel::~Channel() {
    // メモリ管理はサーバークラスで行うため、ここでは特に何もしない
    std::cout << "\033[1;33m[CHANNEL] Destroying channel " << _name << "\033[0m" << std::endl;
}

// ゲッター
std::string Channel::getName() const {
    return _name;
}

std::string Channel::getTopic() const {
    return _topic;
}

std::string Channel::getKey() const {
    return _key;
}

std::vector<Client*> Channel::getClients() const {
    return _clients;
}

bool Channel::isInviteOnly() const {
    return _inviteOnly;
}

bool Channel::isTopicRestricted() const {
    return _topicRestricted;
}

bool Channel::hasKey() const {
    return !_key.empty();
}

bool Channel::hasUserLimit() const {
    return _hasUserLimit;
}

size_t Channel::getUserLimit() const {
    return _userLimit;
}

time_t Channel::getCreationTime() const {
    return _creationTime;
}

// セッター
void Channel::setTopic(const std::string& topic) {
    std::string oldTopic = _topic;
    _topic = topic;

    std::cout << "\033[1;33m[CHANNEL] " << _name << " topic changed";
    if (oldTopic.empty()) {
        std::cout << " to: " << topic;
    } else {
        std::cout << " from: " << oldTopic << " to: " << topic;
    }
    std::cout << "\033[0m" << std::endl;
}

void Channel::setKey(const std::string& key) {
    bool hadKey = hasKey();
    _key = key;

    if (key.empty() && hadKey) {
        std::cout << "\033[1;33m[CHANNEL] " << _name << " key removed\033[0m" << std::endl;
    } else if (!key.empty()) {
        std::cout << "\033[1;33m[CHANNEL] " << _name << " key set to: " << key << "\033[0m" << std::endl;
    }
}

void Channel::setInviteOnly(bool inviteOnly) {
    if (_inviteOnly != inviteOnly) {
        _inviteOnly = inviteOnly;
        std::cout << "\033[1;33m[CHANNEL] " << _name << " invite-only mode "
                  << (inviteOnly ? "enabled" : "disabled") << "\033[0m" << std::endl;
    }
}

void Channel::setTopicRestricted(bool restricted) {
    if (_topicRestricted != restricted) {
        _topicRestricted = restricted;
        std::cout << "\033[1;33m[CHANNEL] " << _name << " topic restriction "
                  << (restricted ? "enabled" : "disabled") << "\033[0m" << std::endl;
    }
}

void Channel::setUserLimit(size_t limit) {
    if (limit > 0) {
        _userLimit = limit;
        _hasUserLimit = true;
        std::cout << "\033[1;33m[CHANNEL] " << _name << " user limit set to " << limit << "\033[0m" << std::endl;
    } else {
        _userLimit = 0;
        _hasUserLimit = false;
        std::cout << "\033[1;33m[CHANNEL] " << _name << " user limit removed\033[0m" << std::endl;
    }
}

// ユーザー管理
bool Channel::addClient(Client* client, const std::string& key) {
    // クライアントのnullチェック
    if (!client) {
        std::cout << "\033[1;31m[ERROR] Attempted to add NULL client to channel " << _name << "\033[0m" << std::endl;
        return false;
    }

    // すでに参加している場合は何もしない
    if (isClientInChannel(client)) {
        std::cout << "\033[1;33m[CHANNEL] Client " << client->getNickname() << " is already in channel " << _name << "\033[0m" << std::endl;
        return true;
    }

    // 無効なニックネームのチェック
    if (client->getNickname().empty()) {
        std::cout << "\033[1;31m[ERROR] Cannot add client with empty nickname to channel " << _name << "\033[0m" << std::endl;
        return false;
    }

    // ニックネーム長のチェック
    if (client->getNickname().length() > 9) {
        std::cout << "\033[1;31m[ERROR] Cannot add client with nickname longer than 9 characters: "
                  << client->getNickname() << "\033[0m" << std::endl;
        return false;
    }

    // キーが設定されている場合はチェック
    if (hasKey() && key != _key) {
        std::cout << "\033[1;33m[CHANNEL] " << _name << " access denied for "
                  << client->getNickname() << ": invalid key\033[0m" << std::endl;
        return false;
    }

    // 招待制の場合は招待されているかチェック
    if (_inviteOnly && !isInvited(client->getNickname())) {
        std::cout << "\033[1;33m[CHANNEL] " << _name << " access denied for "
                  << client->getNickname() << ": not invited\033[0m" << std::endl;
        return false;
    }

    // ユーザー数制限チェック
    if (_hasUserLimit && _clients.size() >= _userLimit) {
        std::cout << "\033[1;33m[CHANNEL] " << _name << " access denied for "
                  << client->getNickname() << ": channel full (" << _clients.size()
                  << "/" << _userLimit << ")\033[0m" << std::endl;
        return false;
    }

    // 過剰なクライアント数のチェック（実装上の制限）
    const size_t MAX_CLIENTS_PER_CHANNEL = 200;
    if (_clients.size() >= MAX_CLIENTS_PER_CHANNEL) {
        std::cout << "\033[1;31m[ERROR] Channel " << _name << " has reached maximum capacity ("
                  << MAX_CLIENTS_PER_CHANNEL << " clients)\033[0m" << std::endl;
        return false;
    }

    // クライアントを追加
    _clients.push_back(client);
    client->addChannel(_name);

    // 招待リストから削除
    removeInvite(client->getNickname());

    std::cout << "\033[1;32m[CHANNEL] " << client->getNickname() << " joined " << _name
              << " (total users: " << _clients.size() << ")\033[0m" << std::endl;
    return true;
}

void Channel::removeClient(Client* client) {
    // NULLチェック
    if (!client) {
        std::cout << "\033[1;31m[ERROR] Attempted to remove NULL client from channel " << _name << "\033[0m" << std::endl;
        return;
    }

    // クライアントをチャンネルから削除
    std::vector<Client*>::iterator it = std::find(_clients.begin(), _clients.end(), client);
    if (it != _clients.end()) {
        std::string nickname = client->getNickname(); // 先にニックネームを取得
        _clients.erase(it);

        std::cout << "\033[1;31m[CHANNEL] Client left " << _name
                  << " (total users: " << _clients.size() << ")\033[0m" << std::endl;

        // オペレータからも削除（先に取得したニックネームを使用）
        if (!nickname.empty()) {
            removeOperator(nickname);
        }
    }
}

bool Channel::isClientInChannel(Client* client) const {
    return std::find(_clients.begin(), _clients.end(), client) != _clients.end();
}

bool Channel::isClientInChannel(const std::string& nickname) const {
    for (std::vector<Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if ((*it)->getNickname() == nickname) {
            return true;
        }
    }
    return false;
}

// オペレータ管理
bool Channel::isOperator(const std::string& nickname) const {
    return std::find(_operators.begin(), _operators.end(), nickname) != _operators.end();
}

void Channel::addOperator(const std::string& nickname) {
    if (!isOperator(nickname)) {
        _operators.push_back(nickname);
        std::cout << "\033[1;35m[CHANNEL] " << nickname << " is now an operator in " << _name << "\033[0m" << std::endl;
    }
}

void Channel::removeOperator(const std::string& nickname) {
    std::vector<std::string>::iterator it = std::find(_operators.begin(), _operators.end(), nickname);
    if (it != _operators.end()) {
        _operators.erase(it);
        std::cout << "\033[1;35m[CHANNEL] " << nickname << " is no longer an operator in " << _name << "\033[0m" << std::endl;
    }
}

// 招待管理
void Channel::inviteUser(const std::string& nickname) {
    if (!isInvited(nickname)) {
        _invitedUsers.push_back(nickname);
        std::cout << "\033[1;33m[CHANNEL] " << nickname << " was invited to " << _name << "\033[0m" << std::endl;
    }
}

bool Channel::isInvited(const std::string& nickname) const {
    return std::find(_invitedUsers.begin(), _invitedUsers.end(), nickname) != _invitedUsers.end();
}

void Channel::removeInvite(const std::string& nickname) {
    std::vector<std::string>::iterator it = std::find(_invitedUsers.begin(), _invitedUsers.end(), nickname);
    if (it != _invitedUsers.end()) {
        _invitedUsers.erase(it);
        std::cout << "\033[1;33m[CHANNEL] Removed " << nickname << " from " << _name << " invite list\033[0m" << std::endl;
    }
}

// メッセージ送信
void Channel::broadcastMessage(const std::string& message, Client* exclude) {
    std::cout << "\033[1;34m[BROADCAST] To channel " << _name << ": " << message << "\033[0m" << std::endl;

    for (std::vector<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (*it != exclude) {
            (*it)->sendMessage(message);
        }
    }
}

void Channel::sendNames(Client* client) {
    std::string namesList;

    for (std::vector<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (isOperator((*it)->getNickname())) {
            namesList += "@" + (*it)->getNickname() + " ";
        } else {
            namesList += (*it)->getNickname() + " ";
        }
    }

    client->sendNumericReply(RPL_NAMREPLY, "= " + _name + " :" + namesList);
    client->sendNumericReply(RPL_ENDOFNAMES, _name + " :End of /NAMES list");
}

// モード管理
std::string Channel::getModes() const {
    std::string modes = "+";

    if (_inviteOnly) {
        modes += "i";
    }
    if (_topicRestricted) {
        modes += "t";
    }
    if (hasKey()) {
        modes += "k";
    }
    if (_hasUserLimit) {
        modes += "l";
    }

    return modes;
}

bool Channel::applyMode(char mode, bool set, const std::string& param, Client* client) {
    std::string clientNick = client ? client->getNickname() : "Unknown";

    switch (mode) {
        case 'i': // 招待制
            setInviteOnly(set);
            std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                      << " mode " << (set ? "+" : "-") << "i\033[0m" << std::endl;
            return true;

        case 't': // トピック制限
            setTopicRestricted(set);
            std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                      << " mode " << (set ? "+" : "-") << "t\033[0m" << std::endl;
            return true;

        case 'k': // キー（パスワード）
            if (set && !param.empty()) {
                setKey(param);
                std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                          << " mode +k " << param << "\033[0m" << std::endl;
                return true;
            } else if (!set) {
                setKey("");
                std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                          << " mode -k\033[0m" << std::endl;
                return true;
            }
            break;

        case 'o': // オペレータ権限
            if (!param.empty()) {
                // ユーザーがチャンネルに参加しているか確認
                if (!isClientInChannel(param)) {
                    std::cout << "\033[1;31m[MODE] " << clientNick << " tried to set " << _name
                              << " mode " << (set ? "+" : "-") << "o for " << param
                              << " but user is not in channel\033[0m" << std::endl;

                    // クライアントがnullでない場合にエラーメッセージを送信
                    if (client) {
                        std::string errorMsg = ":" + std::string(IRC_SERVER_NAME) + " 441 " +
                                               clientNick + " " + param + " " + _name +
                                               " :They aren't on that channel";
                        client->sendMessage(errorMsg);
                    }

                    return false;
                }

                if (set) {
                    addOperator(param);
                    std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                              << " mode +o " << param << "\033[0m" << std::endl;
                } else {
                    removeOperator(param);
                    std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                              << " mode -o " << param << "\033[0m" << std::endl;
                }
                return true;
            }
            break;

        case 'l': // ユーザー数制限
            if (set && !param.empty()) {
                // 数値チェック：すべて数字で、かつ0より大きいことを確認
                bool isValidNumber = true;
                for (size_t i = 0; i < param.length(); i++) {
                    if (!isdigit(param[i])) {
                        isValidNumber = false;
                        break;
                    }
                }

                int limit = 0;
                if (isValidNumber) {
                    limit = std::atoi(param.c_str());
                }

                if (!isValidNumber || limit <= 0) {
                    std::cout << "\033[1;31m[ERROR] Invalid user limit: " << param
                              << " (must be a positive number)\033[0m" << std::endl;
                    return false;
                }

                setUserLimit(limit);
                std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                          << " mode +l " << param << "\033[0m" << std::endl;
                return true;
            } else if (!set) {
                setUserLimit(0);
                std::cout << "\033[1;33m[MODE] " << clientNick << " set " << _name
                          << " mode -l\033[0m" << std::endl;
                return true;
            }
            break;
    }

    return false;
}

// ユーティリティ
size_t Channel::getClientCount() const {
    return _clients.size();
}
