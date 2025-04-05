#include "../include/Channel.hpp"

Channel::Channel(const std::string& name, Client* creator)
    : _name(name), _inviteOnly(false), _topicRestricted(true), _userLimit(0),
      _hasUserLimit(false), _creationTime(time(NULL))
{
    if (creator) {
        _clients.push_back(creator);
        _operators.push_back(creator->getNickname());
    }
}

Channel::~Channel() {
    // メモリ管理はサーバークラスで行うため、ここでは特に何もしない
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
    _topic = topic;
}

void Channel::setKey(const std::string& key) {
    _key = key;
}

void Channel::setInviteOnly(bool inviteOnly) {
    _inviteOnly = inviteOnly;
}

void Channel::setTopicRestricted(bool restricted) {
    _topicRestricted = restricted;
}

void Channel::setUserLimit(size_t limit) {
    if (limit > 0) {
        _userLimit = limit;
        _hasUserLimit = true;
    } else {
        _userLimit = 0;
        _hasUserLimit = false;
    }
}

// ユーザー管理
bool Channel::addClient(Client* client, const std::string& key) {
    // すでに参加している場合は何もしない
    if (isClientInChannel(client)) {
        return true;
    }

    // キーが設定されている場合はチェック
    if (hasKey() && key != _key) {
        return false;
    }

    // 招待制の場合は招待されているかチェック
    if (_inviteOnly && !isInvited(client->getNickname())) {
        return false;
    }

    // ユーザー数制限チェック
    if (_hasUserLimit && _clients.size() >= _userLimit) {
        return false;
    }

    // クライアントを追加
    _clients.push_back(client);
    client->addChannel(_name);

    // 招待リストから削除
    removeInvite(client->getNickname());

    return true;
}

void Channel::removeClient(Client* client) {
    // クライアントをチャンネルから削除
    std::vector<Client*>::iterator it = std::find(_clients.begin(), _clients.end(), client);
    if (it != _clients.end()) {
        _clients.erase(it);
        client->removeChannel(_name);
    }

    // オペレータから削除
    removeOperator(client->getNickname());

    // チャンネルが空になった場合は後でサーバーがチャンネルを削除する
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
    }
}

void Channel::removeOperator(const std::string& nickname) {
    std::vector<std::string>::iterator it = std::find(_operators.begin(), _operators.end(), nickname);
    if (it != _operators.end()) {
        _operators.erase(it);
    }
}

// 招待管理
void Channel::inviteUser(const std::string& nickname) {
    if (!isInvited(nickname)) {
        _invitedUsers.push_back(nickname);
    }
}

bool Channel::isInvited(const std::string& nickname) const {
    return std::find(_invitedUsers.begin(), _invitedUsers.end(), nickname) != _invitedUsers.end();
}

void Channel::removeInvite(const std::string& nickname) {
    std::vector<std::string>::iterator it = std::find(_invitedUsers.begin(), _invitedUsers.end(), nickname);
    if (it != _invitedUsers.end()) {
        _invitedUsers.erase(it);
    }
}

// メッセージ送信
void Channel::broadcastMessage(const std::string& message, Client* exclude) {
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

bool Channel::applyMode(char mode, bool set, const std::string& param, Client*) {
    switch (mode) {
        case 'i': // 招待制
            setInviteOnly(set);
            return true;
        case 't': // トピック制限
            setTopicRestricted(set);
            return true;
        case 'k': // キー（パスワード）
            if (set && !param.empty()) {
                setKey(param);
                return true;
            } else if (!set) {
                setKey("");
                return true;
            }
            break;
        case 'o': // オペレータ権限
            if (!param.empty()) {
                if (set) {
                    addOperator(param);
                } else {
                    removeOperator(param);
                }
                return true;
            }
            break;
        case 'l': // ユーザー数制限
            if (set && !param.empty()) {
                setUserLimit(std::atoi(param.c_str()));
                return true;
            } else if (!set) {
                setUserLimit(0);
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
