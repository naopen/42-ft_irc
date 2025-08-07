/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: na-kannan <na-kannan@student.42tokyo.jp>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/01 00:00:00 by na-kannan         #+#    #+#             */
/*   Updated: 2025/01/01 00:00:00 by na-kannan        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/bonus/Bot.hpp"

/**
 * @brief Botクラスのコンストラクタ
 * @param server サーバーインスタンス
 * @param nickname Botのニックネーム
 * @param realname Botの実名
 */
Bot::Bot(Server* server, const std::string& nickname, const std::string& realname) 
    : _server(server), _nickname(nickname), _realname(realname), _username(nickname), _active(true) {
    // Botの初期化処理
}

/**
 * @brief Botクラスのデストラクタ
 */
Bot::~Bot() {
    // クリーンアップ処理
    _channels.clear();
}

/**
 * @brief クライアントがチャンネルに参加した時の処理
 * @param client 参加したクライアント
 * @param channel 参加先のチャンネル
 */
void Bot::onJoin(Client* client, const std::string& channel) {
    (void)client;
    (void)channel;
    // デフォルトでは何もしない（サブクラスでオーバーライド可能）
}

/**
 * @brief クライアントがチャンネルから退出した時の処理
 * @param client 退出したクライアント
 * @param channel 退出元のチャンネル
 */
void Bot::onPart(Client* client, const std::string& channel) {
    (void)client;
    (void)channel;
    // デフォルトでは何もしない（サブクラスでオーバーライド可能）
}

/**
 * @brief クライアントがサーバーから切断した時の処理
 * @param client 切断したクライアント
 * @param reason 切断理由
 */
void Bot::onQuit(Client* client, const std::string& reason) {
    (void)client;
    (void)reason;
    // デフォルトでは何もしない（サブクラスでオーバーライド可能）
}

/**
 * @brief メッセージを送信
 * @param target 送信先（チャンネルまたはユーザー）
 * @param message メッセージ内容
 */
void Bot::sendMessage(const std::string& target, const std::string& message) {
    if (!_active || !_server)
        return;

    std::string formatted = ":" + _nickname + " PRIVMSG " + target + " :" + message + "\r\n";
    
    // ターゲットがチャンネルの場合
    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = _server->getChannel(target);
        if (channel) {
            std::vector<Client*> members = channel->getClients();
            for (std::vector<Client*>::iterator it = members.begin(); it != members.end(); ++it) {
                (*it)->sendMessage(formatted);
            }
        }
    }
    // ターゲットがユーザーの場合
    else {
        Client* client = _server->getClientByNickname(target);
        if (client) {
            client->sendMessage(formatted);
        }
    }
}

/**
 * @brief プライベートメッセージを送信
 * @param target 送信先クライアント
 * @param message メッセージ内容
 */
void Bot::sendPrivateMessage(Client* target, const std::string& message) {
    if (!target || !_active)
        return;
    
    std::string formatted = ":" + _nickname + " PRIVMSG " + target->getNickname() + " :" + message + "\r\n";
    target->sendMessage(formatted);
}

/**
 * @brief チャンネルにメッセージを送信
 * @param channel チャンネル名
 * @param message メッセージ内容
 */
void Bot::sendChannelMessage(const std::string& channel, const std::string& message) {
    if (!_active || !_server)
        return;
    
    sendMessage(channel, message);
}

/**
 * @brief チャンネルに参加
 * @param channel チャンネル名
 */
void Bot::joinChannel(const std::string& channel) {
    if (!_active || !_server)
        return;
    
    Channel* ch = _server->getChannel(channel);
    if (!ch) {
        _server->createChannel(channel, NULL);
        ch = _server->getChannel(channel);
    }
    
    if (ch) {
        _channels.push_back(channel);
        // チャンネルメンバーに参加通知を送信
        std::string joinMsg = ":" + _nickname + " JOIN " + channel + "\r\n";
        std::vector<Client*> members = ch->getClients();
        for (std::vector<Client*>::iterator it = members.begin(); it != members.end(); ++it) {
            (*it)->sendMessage(joinMsg);
        }
    }
}

/**
 * @brief チャンネルから退出
 * @param channel チャンネル名
 */
void Bot::partChannel(const std::string& channel) {
    if (!_active || !_server)
        return;
    
    // チャンネルリストから削除
    for (std::vector<std::string>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if (*it == channel) {
            _channels.erase(it);
            break;
        }
    }
    
    // チャンネルメンバーに退出通知を送信
    Channel* ch = _server->getChannel(channel);
    if (ch) {
        std::string partMsg = ":" + _nickname + " PART " + channel + " :Leaving\r\n";
        std::vector<Client*> members = ch->getClients();
        for (std::vector<Client*>::iterator it = members.begin(); it != members.end(); ++it) {
            (*it)->sendMessage(partMsg);
        }
    }
}

/**
 * @brief NOTICEメッセージを送信
 * @param target 送信先
 * @param message メッセージ内容
 */
void Bot::sendNotice(const std::string& target, const std::string& message) {
    if (!_active || !_server)
        return;
    
    std::string formatted = ":" + _nickname + " NOTICE " + target + " :" + message + "\r\n";
    
    // ターゲットがチャンネルの場合
    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = _server->getChannel(target);
        if (channel) {
            std::vector<Client*> members = channel->getClients();
            for (std::vector<Client*>::iterator it = members.begin(); it != members.end(); ++it) {
                (*it)->sendMessage(formatted);
            }
        }
    }
    // ターゲットがユーザーの場合
    else {
        Client* client = _server->getClientByNickname(target);
        if (client) {
            client->sendMessage(formatted);
        }
    }
}

// ゲッター
std::string Bot::getNickname() const { return _nickname; }
std::string Bot::getRealname() const { return _realname; }
std::string Bot::getUsername() const { return _username; }
bool Bot::isActive() const { return _active; }
Server* Bot::getServer() const { return _server; }

// セッター
void Bot::setActive(bool active) { _active = active; }
void Bot::setNickname(const std::string& nickname) { _nickname = nickname; }
