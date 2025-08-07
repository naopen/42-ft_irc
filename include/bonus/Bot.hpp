/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: na-kannan <na-kannan@student.42tokyo.jp>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/01 00:00:00 by na-kannan         #+#    #+#             */
/*   Updated: 2025/01/01 00:00:00 by na-kannan        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BOT_HPP
# define BOT_HPP

# include "../Utils.hpp"
# include "../Client.hpp"
# include "../Channel.hpp"
# include "../Server.hpp"
# include <sstream>
# include <cstdlib>
# include <ctime>

class Server;
class Client;
class Channel;

// Botの基底クラス
class Bot {
protected:
    Server*                     _server;
    std::string                 _nickname;
    std::string                 _realname;
    std::string                 _username;
    bool                        _active;
    std::vector<std::string>    _channels;

public:
    Bot(Server* server, const std::string& nickname, const std::string& realname);
    virtual ~Bot();

    // 基本的なメソッド
    virtual void    onMessage(Client* sender, const std::string& target, const std::string& message) = 0;
    virtual void    onPrivateMessage(Client* sender, const std::string& message) = 0;
    virtual void    onChannelMessage(Client* sender, const std::string& channel, const std::string& message) = 0;
    virtual void    onJoin(Client* client, const std::string& channel);
    virtual void    onPart(Client* client, const std::string& channel);
    virtual void    onQuit(Client* client, const std::string& reason);

    // ヘルパーメソッド
    void            sendMessage(const std::string& target, const std::string& message);
    void            sendPrivateMessage(Client* target, const std::string& message);
    void            sendChannelMessage(const std::string& channel, const std::string& message);
    void            joinChannel(const std::string& channel);
    void            partChannel(const std::string& channel);
    void            sendNotice(const std::string& target, const std::string& message);

    // ゲッター
    std::string     getNickname() const;
    std::string     getRealname() const;
    std::string     getUsername() const;
    bool            isActive() const;
    Server*         getServer() const;

    // セッター
    void            setActive(bool active);
    void            setNickname(const std::string& nickname);
};

#endif
