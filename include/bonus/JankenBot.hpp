/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   JankenBot.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: na-kannan <na-kannan@student.42tokyo.jp>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/01 00:00:00 by na-kannan         #+#    #+#             */
/*   Updated: 2025/01/01 00:00:00 by na-kannan        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef JANKENBOT_HPP
# define JANKENBOT_HPP

# include "Bot.hpp"
# include <map>

// じゃんけんの手
enum JankenHand {
    ROCK = 0,
    SCISSORS = 1,
    PAPER = 2,
    INVALID = -1
};

// ゲームの状態
struct JankenGame {
    Client*         player;
    JankenHand      playerHand;
    JankenHand      botHand;
    int             playerScore;
    int             botScore;
    bool            waitingForHand;
    time_t          lastActivity;
};

// じゃんけんBot
class JankenBot : public Bot {
private:
    std::map<std::string, JankenGame>  _games;  // プレイヤーごとのゲーム状態
    std::map<std::string, int>         _stats;  // 統計情報（勝利数）

    // ヘルパーメソッド
    JankenHand      parseHand(const std::string& hand) const;
    std::string     handToString(JankenHand hand) const;
    std::string     handToEmoji(JankenHand hand) const;
    JankenHand      generateBotHand() const;
    int             determineWinner(JankenHand player, JankenHand bot) const;
    void            startNewGame(Client* player);
    void            processHand(Client* player, const std::string& hand);
    void            showHelp(Client* player);
    void            showStats(Client* player);
    void            resetGame(Client* player);
    void            cleanupInactiveGames();

public:
    JankenBot(Server* server);
    ~JankenBot();

    // Bot基底クラスの純粋仮想関数を実装
    void    onMessage(Client* sender, const std::string& target, const std::string& message);
    void    onPrivateMessage(Client* sender, const std::string& message);
    void    onChannelMessage(Client* sender, const std::string& channel, const std::string& message);
    void    onJoin(Client* client, const std::string& channel);
};

#endif
