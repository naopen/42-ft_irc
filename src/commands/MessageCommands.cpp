#include "../../include/Command.hpp"
#include "../../include/Server.hpp"

// PRIVMSG コマンド
PrivmsgCommand::PrivmsgCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "PRIVMSG", params)
{
}

PrivmsgCommand::~PrivmsgCommand() {
    // 特に何もしない
}

void PrivmsgCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NORECIPIENT, ":No recipient given (PRIVMSG)");
        return;
    }

    if (_params.size() < 2) {
        _client->sendNumericReply(ERR_NOTEXTTOSEND, ":No text to send");
        return;
    }

    std::string target = _params[0];
    std::string message = _params[1];

    // デバッグ出力
    std::cout << "PRIVMSG from " << _client->getNickname() << " to " << target << ": " << message << std::endl;

    // 宛先が複数の場合はカンマで区切られている
    std::vector<std::string> targets = Utils::split(target, ',');

    for (std::vector<std::string>::iterator it = targets.begin(); it != targets.end(); ++it) {
        std::string currentTarget = *it;

        // チャンネルへのメッセージ
        if (currentTarget[0] == CHANNEL_PREFIX) {
            // チャンネルが存在するか確認
            if (!_server->channelExists(currentTarget)) {
                _client->sendNumericReply(ERR_NOSUCHCHANNEL, currentTarget + " :No such channel");
                std::cout << "Channel does not exist: " << currentTarget << std::endl;
                continue;
            }

            Channel* channel = _server->getChannel(currentTarget);

            // クライアントがチャンネルに参加しているか確認
            if (!channel->isClientInChannel(_client)) {
                _client->sendNumericReply(ERR_CANNOTSENDTOCHAN, currentTarget + " :Cannot send to channel");
                std::cout << "Client not in channel: " << currentTarget << std::endl;
                continue;
            }

            // メッセージを整形
            std::string formattedMessage = ":" + _client->getPrefix() + " PRIVMSG " + currentTarget + " :" + message;
            std::cout << "Broadcasting to channel: " << formattedMessage << std::endl;

            // クライアント自身以外の全メンバーにメッセージを送信
            channel->broadcastMessage(formattedMessage, _client);
        }
        // ユーザーへのメッセージ
        else {
            // ユーザーが存在するか確認
            Client* targetClient = _server->getClientByNickname(currentTarget);
            if (!targetClient) {
                _client->sendNumericReply(ERR_NOSUCHNICK, currentTarget + " :No such nick/channel");
                std::cout << "Target user not found: " << currentTarget << std::endl;
                continue;
            }

            // メッセージを整形
            std::string formattedMessage = ":" + _client->getPrefix() + " PRIVMSG " + currentTarget + " :" + message;
            std::cout << "Sending to user: " << formattedMessage << std::endl;

            // ターゲットユーザーにメッセージを送信
            targetClient->sendMessage(formattedMessage);

            // ターゲットユーザーが離席中の場合は通知
            if (targetClient->isAway()) {
                _client->sendNumericReply(RPL_AWAY, targetClient->getNickname() + " :" + targetClient->getAwayMessage());
            }
        }
    }
}

// NOTICE コマンド
NoticeCommand::NoticeCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "NOTICE", params)
{
}

NoticeCommand::~NoticeCommand() {
    // 特に何もしない
}

void NoticeCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー（NOTICEはエラーを返さない）
    if (_params.size() < 2) {
        return;
    }

    std::string target = _params[0];
    std::string message = _params[1];

    // 宛先が複数の場合はカンマで区切られている
    std::vector<std::string> targets = Utils::split(target, ',');

    for (std::vector<std::string>::iterator it = targets.begin(); it != targets.end(); ++it) {
        std::string currentTarget = *it;

        // チャンネルへのメッセージ
        if (currentTarget[0] == CHANNEL_PREFIX) {
            // チャンネルが存在するか確認
            if (!_server->channelExists(currentTarget)) {
                continue; // NOTICEはエラーを返さない
            }

            Channel* channel = _server->getChannel(currentTarget);

            // クライアントがチャンネルに参加しているか確認
            if (!channel->isClientInChannel(_client)) {
                continue; // NOTICEはエラーを返さない
            }

            // メッセージを整形
            std::string formattedMessage = ":" + _client->getPrefix() + " NOTICE " + currentTarget + " :" + message;

            // クライアント自身以外の全メンバーにメッセージを送信
            channel->broadcastMessage(formattedMessage, _client);
        }
        // ユーザーへのメッセージ
        else {
            // ユーザーが存在するか確認
            Client* targetClient = _server->getClientByNickname(currentTarget);
            if (!targetClient) {
                continue; // NOTICEはエラーを返さない
            }

            // メッセージを整形
            std::string formattedMessage = ":" + _client->getPrefix() + " NOTICE " + currentTarget + " :" + message;

            // ターゲットユーザーにメッセージを送信
            targetClient->sendMessage(formattedMessage);
        }
    }
}