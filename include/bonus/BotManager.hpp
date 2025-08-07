/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   BotManager.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: na-kannan <na-kannan@student.42tokyo.jp>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/01 00:00:00 by na-kannan         #+#    #+#             */
/*   Updated: 2025/01/01 00:00:00 by na-kannan        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BOTMANAGER_HPP
# define BOTMANAGER_HPP

# include <vector>
# include <map>

class Server;
class Client;
class Bot;

/**
 * @brief Bot管理クラス
 * 複数のBotインスタンスを管理し、メッセージをルーティングする
 */
class BotManager {
private:
    Server*                         _server;
    std::map<std::string, Bot*>    _bots;      // ニックネーム -> Botインスタンス
    bool                            _enabled;   // Bot機能の有効/無効

public:
    BotManager(Server* server);
    ~BotManager();

    // Bot管理
    void        addBot(Bot* bot);
    void        removeBot(const std::string& nickname);
    Bot*        getBot(const std::string& nickname);
    bool        isBotNickname(const std::string& nickname) const;
    
    // メッセージルーティング
    void        routeMessage(Client* sender, const std::string& target, const std::string& message);
    void        routePrivateMessage(Client* sender, const std::string& botNick, const std::string& message);
    void        routeChannelMessage(Client* sender, const std::string& channel, const std::string& message);
    
    // イベントハンドリング
    void        handleJoin(Client* client, const std::string& channel);
    void        handlePart(Client* client, const std::string& channel);
    void        handleQuit(Client* client, const std::string& reason);
    
    // Bot初期化
    void        initializeBots();
    void        shutdownBots();
    
    // 状態管理
    void        setEnabled(bool enabled);
    bool        isEnabled() const;
    
    // デバッグ用
    void        listBots() const;
};

#endif
