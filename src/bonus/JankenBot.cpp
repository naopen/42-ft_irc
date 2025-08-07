/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   JankenBot.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: na-kannan <na-kannan@student.42tokyo.jp>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/01 00:00:00 by na-kannan         #+#    #+#             */
/*   Updated: 2025/01/01 00:00:00 by na-kannan        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/bonus/JankenBot.hpp"

/**
 * @brief JankenBotのコンストラクタ
 * @param server サーバーインスタンス
 */
JankenBot::JankenBot(Server* server) 
    : Bot(server, "JankenBot", "Rock Paper Scissors Bot") {
    // 乱数のシード初期化
    std::srand(std::time(NULL));
}

/**
 * @brief JankenBotのデストラクタ
 */
JankenBot::~JankenBot() {
    _games.clear();
    _stats.clear();
}

/**
 * @brief メッセージを受信した時の処理
 * @param sender 送信者
 * @param target 送信先
 * @param message メッセージ内容
 */
void JankenBot::onMessage(Client* sender, const std::string& target, const std::string& message) {
    if (!sender || !_active)
        return;
    
    // Botへのプライベートメッセージ
    if (target == _nickname) {
        onPrivateMessage(sender, message);
    }
    // チャンネルへのメッセージ
    else if (target[0] == '#' || target[0] == '&') {
        onChannelMessage(sender, target, message);
    }
}

/**
 * @brief プライベートメッセージを受信した時の処理
 * @param sender 送信者
 * @param message メッセージ内容
 */
void JankenBot::onPrivateMessage(Client* sender, const std::string& message) {
    if (!sender)
        return;
    
    std::string msg = message;
    // メッセージをトリミング
    size_t start = msg.find_first_not_of(" \t");
    size_t end = msg.find_last_not_of(" \t\r\n");
    if (start != std::string::npos && end != std::string::npos)
        msg = msg.substr(start, end - start + 1);
    
    // コマンドを小文字に変換
    std::string cmd = msg;
    for (size_t i = 0; i < cmd.length(); ++i) {
        cmd[i] = std::tolower(cmd[i]);
    }
    
    // コマンド処理
    if (cmd == "help" || cmd == "!help") {
        showHelp(sender);
    }
    else if (cmd == "start" || cmd == "!start" || cmd == "play" || cmd == "!play") {
        startNewGame(sender);
    }
    else if (cmd == "stats" || cmd == "!stats" || cmd == "score" || cmd == "!score") {
        showStats(sender);
    }
    else if (cmd == "reset" || cmd == "!reset" || cmd == "quit" || cmd == "!quit") {
        resetGame(sender);
    }
    else if (cmd == "rock" || cmd == "r" || cmd == "gu" || cmd == "グー") {
        processHand(sender, "rock");
    }
    else if (cmd == "scissors" || cmd == "s" || cmd == "choki" || cmd == "チョキ") {
        processHand(sender, "scissors");
    }
    else if (cmd == "paper" || cmd == "p" || cmd == "pa" || cmd == "パー") {
        processHand(sender, "paper");
    }
    else {
        // ゲーム中でない場合はヘルプを表示
        std::map<std::string, JankenGame>::iterator it = _games.find(sender->getNickname());
        if (it == _games.end() || !it->second.waitingForHand) {
            sendPrivateMessage(sender, "Hello! I'm JankenBot. Type 'help' to see available commands.");
        } else {
            sendPrivateMessage(sender, "Invalid hand! Please choose: rock (r), scissors (s), or paper (p)");
        }
    }
}

/**
 * @brief チャンネルメッセージを受信した時の処理
 * @param sender 送信者
 * @param channel チャンネル名
 * @param message メッセージ内容
 */
void JankenBot::onChannelMessage(Client* sender, const std::string& channel, const std::string& message) {
    if (!sender)
        return;
    
    // Bot宛のメンションをチェック
    std::string mention = _nickname + ":";
    std::string mention2 = "@" + _nickname;
    
    if (message.find(mention) == 0 || message.find(mention2) == 0) {
        size_t pos = (message.find(mention) == 0) ? mention.length() : mention2.length();
        std::string cmd = message.substr(pos);
        
        // トリミング
        size_t start = cmd.find_first_not_of(" \t");
        if (start != std::string::npos)
            cmd = cmd.substr(start);
        
        // チャンネルで応答
        if (cmd == "help") {
            sendChannelMessage(channel, sender->getNickname() + ": I'm JankenBot! Send me a private message to play Rock-Paper-Scissors!");
        }
        else {
            sendChannelMessage(channel, sender->getNickname() + ": Please send me a private message to play!");
        }
    }
    
    // !jankenコマンドをチャンネルで検出
    if (message == "!janken" || message == "!rps") {
        sendChannelMessage(channel, "🎮 Rock-Paper-Scissors Bot is here! Send me a private message to play!");
    }
}

/**
 * @brief クライアントがチャンネルに参加した時の処理
 * @param client 参加したクライアント
 * @param channel 参加先のチャンネル
 */
void JankenBot::onJoin(Client* client, const std::string& channel) {
    if (!client || client->getNickname() == _nickname)
        return;
    
    // ウェルカムメッセージを送信
    sendNotice(client->getNickname(), "Welcome to " + channel + "! I'm JankenBot. Send me a private message to play Rock-Paper-Scissors!");
}

/**
 * @brief 文字列をじゃんけんの手に変換
 * @param hand 手の文字列
 * @return じゃんけんの手
 */
JankenHand JankenBot::parseHand(const std::string& hand) const {
    std::string h = hand;
    for (size_t i = 0; i < h.length(); ++i) {
        h[i] = std::tolower(h[i]);
    }
    
    if (h == "rock" || h == "r" || h == "gu")
        return ROCK;
    if (h == "scissors" || h == "s" || h == "choki")
        return SCISSORS;
    if (h == "paper" || h == "p" || h == "pa")
        return PAPER;
    
    return INVALID;
}

/**
 * @brief じゃんけんの手を文字列に変換
 * @param hand じゃんけんの手
 * @return 手の文字列
 */
std::string JankenBot::handToString(JankenHand hand) const {
    switch (hand) {
        case ROCK:     return "Rock";
        case SCISSORS: return "Scissors";
        case PAPER:    return "Paper";
        default:       return "Invalid";
    }
}

/**
 * @brief じゃんけんの手を絵文字に変換
 * @param hand じゃんけんの手
 * @return 手の絵文字
 */
std::string JankenBot::handToEmoji(JankenHand hand) const {
    switch (hand) {
        case ROCK:     return "✊";
        case SCISSORS: return "✌️";
        case PAPER:    return "✋";
        default:       return "❓";
    }
}

/**
 * @brief Botの手を生成
 * @return Botの手
 */
JankenHand JankenBot::generateBotHand() const {
    return static_cast<JankenHand>(std::rand() % 3);
}

/**
 * @brief 勝敗を判定
 * @param player プレイヤーの手
 * @param bot Botの手
 * @return 0: 引き分け, 1: プレイヤーの勝ち, -1: Botの勝ち
 */
int JankenBot::determineWinner(JankenHand player, JankenHand bot) const {
    if (player == bot)
        return 0;  // 引き分け
    
    if ((player == ROCK && bot == SCISSORS) ||
        (player == SCISSORS && bot == PAPER) ||
        (player == PAPER && bot == ROCK))
        return 1;  // プレイヤーの勝ち
    
    return -1;  // Botの勝ち
}

/**
 * @brief 新しいゲームを開始
 * @param player プレイヤー
 */
void JankenBot::startNewGame(Client* player) {
    if (!player)
        return;
    
    std::string nickname = player->getNickname();
    
    // 既存のゲームをチェック
    std::map<std::string, JankenGame>::iterator it = _games.find(nickname);
    if (it != _games.end() && it->second.waitingForHand) {
        sendPrivateMessage(player, "You already have a game in progress! Choose your hand: rock (r), scissors (s), or paper (p)");
        return;
    }
    
    // 新しいゲームを作成
    JankenGame game;
    game.player = player;
    game.playerScore = 0;
    game.botScore = 0;
    game.waitingForHand = true;
    game.lastActivity = std::time(NULL);
    
    // 既存のゲームがある場合はスコアを引き継ぐ
    if (it != _games.end()) {
        game.playerScore = it->second.playerScore;
        game.botScore = it->second.botScore;
    }
    
    _games[nickname] = game;
    
    sendPrivateMessage(player, "=== 🎮 Rock-Paper-Scissors Game Started! ===");
    sendPrivateMessage(player, "Current Score - You: " + Utils::toString(game.playerScore) + " | Bot: " + Utils::toString(game.botScore));
    sendPrivateMessage(player, "Choose your hand: rock (r), scissors (s), or paper (p)");
}

/**
 * @brief プレイヤーの手を処理
 * @param player プレイヤー
 * @param hand 手の文字列
 */
void JankenBot::processHand(Client* player, const std::string& hand) {
    if (!player)
        return;
    
    std::string nickname = player->getNickname();
    
    // ゲームが存在するかチェック
    std::map<std::string, JankenGame>::iterator it = _games.find(nickname);
    if (it == _games.end() || !it->second.waitingForHand) {
        sendPrivateMessage(player, "No game in progress! Type 'start' to begin a new game.");
        return;
    }
    
    // 手を解析
    JankenHand playerHand = parseHand(hand);
    if (playerHand == INVALID) {
        sendPrivateMessage(player, "Invalid hand! Please choose: rock (r), scissors (s), or paper (p)");
        return;
    }
    
    // Botの手を生成
    JankenHand botHand = generateBotHand();
    
    // ゲーム情報を更新
    it->second.playerHand = playerHand;
    it->second.botHand = botHand;
    it->second.lastActivity = std::time(NULL);
    it->second.waitingForHand = false;
    
    // 結果を表示
    sendPrivateMessage(player, "=== Round Result ===");
    sendPrivateMessage(player, "You: " + handToString(playerHand) + " " + handToEmoji(playerHand));
    sendPrivateMessage(player, "Bot: " + handToString(botHand) + " " + handToEmoji(botHand));
    
    // 勝敗判定
    int result = determineWinner(playerHand, botHand);
    if (result == 0) {
        sendPrivateMessage(player, "🤝 It's a TIE!");
    }
    else if (result == 1) {
        it->second.playerScore++;
        sendPrivateMessage(player, "🎉 You WIN this round!");
        
        // 統計を更新
        _stats[nickname]++;
    }
    else {
        it->second.botScore++;
        sendPrivateMessage(player, "😔 You LOSE this round!");
    }
    
    // 現在のスコアを表示
    sendPrivateMessage(player, "Score - You: " + Utils::toString(it->second.playerScore) + " | Bot: " + Utils::toString(it->second.botScore));
    
    // 次のラウンドの準備
    it->second.waitingForHand = true;
    sendPrivateMessage(player, "Ready for next round? Choose: rock (r), scissors (s), or paper (p) [or 'quit' to stop]");
}

/**
 * @brief ヘルプメッセージを表示
 * @param player プレイヤー
 */
void JankenBot::showHelp(Client* player) {
    if (!player)
        return;
    
    sendPrivateMessage(player, "=== 🎮 JankenBot Help ===");
    sendPrivateMessage(player, "Commands:");
    sendPrivateMessage(player, "  start/play - Start a new game");
    sendPrivateMessage(player, "  rock/r     - Play Rock ✊");
    sendPrivateMessage(player, "  scissors/s - Play Scissors ✌️");
    sendPrivateMessage(player, "  paper/p    - Play Paper ✋");
    sendPrivateMessage(player, "  stats      - Show your statistics");
    sendPrivateMessage(player, "  reset/quit - Reset the current game");
    sendPrivateMessage(player, "  help       - Show this help message");
    sendPrivateMessage(player, "=======================");
}

/**
 * @brief 統計情報を表示
 * @param player プレイヤー
 */
void JankenBot::showStats(Client* player) {
    if (!player)
        return;
    
    std::string nickname = player->getNickname();
    
    sendPrivateMessage(player, "=== 📊 Your Statistics ===");
    
    // 現在のゲーム
    std::map<std::string, JankenGame>::iterator gameIt = _games.find(nickname);
    if (gameIt != _games.end()) {
        sendPrivateMessage(player, "Current Session:");
        sendPrivateMessage(player, "  Your Score: " + Utils::toString(gameIt->second.playerScore));
        sendPrivateMessage(player, "  Bot Score:  " + Utils::toString(gameIt->second.botScore));
    }
    
    // 総合統計
    std::map<std::string, int>::iterator statIt = _stats.find(nickname);
    int totalWins = (statIt != _stats.end()) ? statIt->second : 0;
    sendPrivateMessage(player, "Total Wins: " + Utils::toString(totalWins));
    sendPrivateMessage(player, "========================");
}

/**
 * @brief ゲームをリセット
 * @param player プレイヤー
 */
void JankenBot::resetGame(Client* player) {
    if (!player)
        return;
    
    std::string nickname = player->getNickname();
    
    std::map<std::string, JankenGame>::iterator it = _games.find(nickname);
    if (it != _games.end()) {
        sendPrivateMessage(player, "Game reset! Final Score - You: " + Utils::toString(it->second.playerScore) + 
                                    " | Bot: " + Utils::toString(it->second.botScore));
        _games.erase(it);
    }
    else {
        sendPrivateMessage(player, "No game in progress.");
    }
    
    sendPrivateMessage(player, "Thanks for playing! Type 'start' to play again.");
}

/**
 * @brief 非アクティブなゲームをクリーンアップ
 */
void JankenBot::cleanupInactiveGames() {
    time_t now = std::time(NULL);
    std::map<std::string, JankenGame>::iterator it = _games.begin();
    
    while (it != _games.end()) {
        // 5分以上アクティビティがない場合は削除
        if (now - it->second.lastActivity > 300) {
            std::map<std::string, JankenGame>::iterator toErase = it;
            ++it;
            _games.erase(toErase);
        }
        else {
            ++it;
        }
    }
}
