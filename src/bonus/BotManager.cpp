/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BotManager.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: na-kannan <na-kannan@student.42tokyo.jp>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/01 00:00:00 by na-kannan         #+#    #+#             */
/*   Updated: 2025/01/01 00:00:00 by na-kannan        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/bonus/BotManager.hpp"
#include "../include/bonus/Bot.hpp"
#include "../include/bonus/JankenBot.hpp"
#include <iostream>

/**
 * @brief BotManagerのコンストラクタ
 * @param server サーバーインスタンス
 */
BotManager::BotManager(Server* server) : _server(server), _enabled(true) {
    // BotManagerの初期化
}

/**
 * @brief BotManagerのデストラクタ
 */
BotManager::~BotManager() {
    shutdownBots();
}

/**
 * @brief Botを追加
 * @param bot 追加するBotインスタンス
 */
void BotManager::addBot(Bot* bot) {
    if (!bot)
        return;
    
    std::string nickname = bot->getNickname();
    
    // 既存のBotをチェック
    if (_bots.find(nickname) != _bots.end()) {
        std::cout << "[BotManager] Bot with nickname '" << nickname << "' already exists" << std::endl;
        return;
    }
    
    _bots[nickname] = bot;
    std::cout << "[BotManager] Added bot: " << nickname << std::endl;
}

/**
 * @brief Botを削除
 * @param nickname 削除するBotのニックネーム
 */
void BotManager::removeBot(const std::string& nickname) {
    std::map<std::string, Bot*>::iterator it = _bots.find(nickname);
    if (it != _bots.end()) {
        delete it->second;
        _bots.erase(it);
        std::cout << "[BotManager] Removed bot: " << nickname << std::endl;
    }
}

/**
 * @brief Botを取得
 * @param nickname Botのニックネーム
 * @return Botインスタンス（存在しない場合はNULL）
 */
Bot* BotManager::getBot(const std::string& nickname) {
    std::map<std::string, Bot*>::iterator it = _bots.find(nickname);
    if (it != _bots.end())
        return it->second;
    return NULL;
}

/**
 * @brief 指定されたニックネームがBotかどうかをチェック
 * @param nickname チェックするニックネーム
 * @return Botの場合true
 */
bool BotManager::isBotNickname(const std::string& nickname) const {
    return _bots.find(nickname) != _bots.end();
}

/**
 * @brief メッセージをルーティング
 * @param sender 送信者
 * @param target 送信先
 * @param message メッセージ内容
 */
void BotManager::routeMessage(Client* sender, const std::string& target, const std::string& message) {
    if (!_enabled || !sender)
        return;
    
    // ターゲットがBotの場合
    Bot* bot = getBot(target);
    if (bot) {
        bot->onMessage(sender, target, message);
        return;
    }
    
    // チャンネルメッセージの場合、全てのBotに通知
    if (target[0] == '#' || target[0] == '&') {
        for (std::map<std::string, Bot*>::iterator it = _bots.begin(); it != _bots.end(); ++it) {
            it->second->onChannelMessage(sender, target, message);
        }
    }
}

/**
 * @brief プライベートメッセージをルーティング
 * @param sender 送信者
 * @param botNick Botのニックネーム
 * @param message メッセージ内容
 */
void BotManager::routePrivateMessage(Client* sender, const std::string& botNick, const std::string& message) {
    if (!_enabled || !sender)
        return;
    
    Bot* bot = getBot(botNick);
    if (bot) {
        bot->onPrivateMessage(sender, message);
    }
}

/**
 * @brief チャンネルメッセージをルーティング
 * @param sender 送信者
 * @param channel チャンネル名
 * @param message メッセージ内容
 */
void BotManager::routeChannelMessage(Client* sender, const std::string& channel, const std::string& message) {
    if (!_enabled || !sender)
        return;
    
    // 全てのBotにチャンネルメッセージを通知
    for (std::map<std::string, Bot*>::iterator it = _bots.begin(); it != _bots.end(); ++it) {
        it->second->onChannelMessage(sender, channel, message);
    }
}

/**
 * @brief クライアントのチャンネル参加を処理
 * @param client 参加したクライアント
 * @param channel チャンネル名
 */
void BotManager::handleJoin(Client* client, const std::string& channel) {
    if (!_enabled || !client)
        return;
    
    // 全てのBotに通知
    for (std::map<std::string, Bot*>::iterator it = _bots.begin(); it != _bots.end(); ++it) {
        it->second->onJoin(client, channel);
    }
}

/**
 * @brief クライアントのチャンネル退出を処理
 * @param client 退出したクライアント
 * @param channel チャンネル名
 */
void BotManager::handlePart(Client* client, const std::string& channel) {
    if (!_enabled || !client)
        return;
    
    // 全てのBotに通知
    for (std::map<std::string, Bot*>::iterator it = _bots.begin(); it != _bots.end(); ++it) {
        it->second->onPart(client, channel);
    }
}

/**
 * @brief クライアントの切断を処理
 * @param client 切断したクライアント
 * @param reason 切断理由
 */
void BotManager::handleQuit(Client* client, const std::string& reason) {
    if (!_enabled || !client)
        return;
    
    // 全てのBotに通知
    for (std::map<std::string, Bot*>::iterator it = _bots.begin(); it != _bots.end(); ++it) {
        it->second->onQuit(client, reason);
    }
}

/**
 * @brief Botを初期化
 */
void BotManager::initializeBots() {
    std::cout << "[BotManager] Initializing bots..." << std::endl;
    
    // じゃんけんBotを追加
    JankenBot* jankenBot = new JankenBot(_server);
    addBot(jankenBot);
    
    // 他のBotをここに追加可能
    
    std::cout << "[BotManager] Initialized " << _bots.size() << " bot(s)" << std::endl;
}

/**
 * @brief 全てのBotをシャットダウン
 */
void BotManager::shutdownBots() {
    std::cout << "[BotManager] Shutting down bots..." << std::endl;
    
    for (std::map<std::string, Bot*>::iterator it = _bots.begin(); it != _bots.end(); ++it) {
        delete it->second;
    }
    _bots.clear();
    
    std::cout << "[BotManager] All bots shut down" << std::endl;
}

/**
 * @brief Bot機能の有効/無効を設定
 * @param enabled 有効にする場合true
 */
void BotManager::setEnabled(bool enabled) {
    _enabled = enabled;
    std::cout << "[BotManager] Bot feature " << (enabled ? "enabled" : "disabled") << std::endl;
}

/**
 * @brief Bot機能が有効かチェック
 * @return 有効な場合true
 */
bool BotManager::isEnabled() const {
    return _enabled;
}

/**
 * @brief 登録されているBotをリスト表示
 */
void BotManager::listBots() const {
    std::cout << "[BotManager] Active bots:" << std::endl;
    for (std::map<std::string, Bot*>::const_iterator it = _bots.begin(); it != _bots.end(); ++it) {
        std::cout << "  - " << it->first << " (" << it->second->getRealname() << ")" << std::endl;
    }
}
