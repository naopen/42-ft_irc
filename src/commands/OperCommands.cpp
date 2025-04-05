#include "../../include/Command.hpp"
#include "../../include/Server.hpp"

// KICK コマンド
KickCommand::KickCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "KICK", params)
{
}

KickCommand::~KickCommand() {
    // 特に何もしない
}

void KickCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.size() < 2) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }

    std::string channelName = _params[0];
    std::string targetNick = _params[1];

    // チャンネル名の先頭に # がない場合は追加
    if (channelName[0] != CHANNEL_PREFIX) {
        channelName = CHANNEL_PREFIX + channelName;
    }

    // チャンネルが存在するか確認
    if (!_server->channelExists(channelName)) {
        _client->sendNumericReply(ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Channel* channel = _server->getChannel(channelName);

    // クライアントがチャンネルに参加しているか確認
    if (!channel->isClientInChannel(_client)) {
        _client->sendNumericReply(ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    // クライアントがチャンネルオペレータかどうか確認
    if (!channel->isOperator(_client->getNickname())) {
        _client->sendNumericReply(ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    // ターゲットユーザーがチャンネルに参加しているか確認
    if (!channel->isClientInChannel(targetNick)) {
        _client->sendNumericReply(ERR_USERNOTINCHANNEL, targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }

    // 退出理由
    std::string reason = "";
    if (_params.size() > 2) {
        reason = _params[2];
    } else {
        reason = targetNick;
    }

    // ターゲットユーザーを取得
    Client* targetClient = _server->getClientByNickname(targetNick);

    // KICKメッセージをブロードキャスト
    std::string kickMessage = ":" + _client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + reason;
    channel->broadcastMessage(kickMessage);

    // ターゲットユーザーをチャンネルから削除
    channel->removeClient(targetClient);
}

// INVITE コマンド
InviteCommand::InviteCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "INVITE", params)
{
}

InviteCommand::~InviteCommand() {
    // 特に何もしない
}

void InviteCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.size() < 2) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
        return;
    }

    std::string targetNick = _params[0];
    std::string channelName = _params[1];

    // チャンネル名の先頭に # がない場合は追加
    if (channelName[0] != CHANNEL_PREFIX) {
        channelName = CHANNEL_PREFIX + channelName;
    }

    // チャンネルが存在するか確認
    if (!_server->channelExists(channelName)) {
        _client->sendNumericReply(ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Channel* channel = _server->getChannel(channelName);

    // クライアントがチャンネルに参加しているか確認
    if (!channel->isClientInChannel(_client)) {
        _client->sendNumericReply(ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    // 招待制チャンネルの場合はオペレータ権限が必要
    if (channel->isInviteOnly() && !channel->isOperator(_client->getNickname())) {
        _client->sendNumericReply(ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    // ターゲットユーザーが存在するか確認
    Client* targetClient = _server->getClientByNickname(targetNick);
    if (!targetClient) {
        _client->sendNumericReply(ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }

    // ターゲットユーザーが既にチャンネルに参加しているか確認
    if (channel->isClientInChannel(targetClient)) {
        _client->sendNumericReply(ERR_USERONCHANNEL, targetNick + " " + channelName + " :is already on channel");
        return;
    }

    // ターゲットユーザーを招待
    channel->inviteUser(targetNick);

    // 招待者に成功メッセージを送信
    _client->sendNumericReply(RPL_INVITING, targetNick + " " + channelName);

    // 招待対象者に招待メッセージを送信
    std::string inviteMessage = ":" + _client->getPrefix() + " INVITE " + targetNick + " :" + channelName;
    targetClient->sendMessage(inviteMessage);
}

// TOPIC コマンド
TopicCommand::TopicCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "TOPIC", params)
{
}

TopicCommand::~TopicCommand() {
    // 特に何もしない
}

void TopicCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }

    std::string channelName = _params[0];

    // チャンネル名の先頭に # がない場合は追加
    if (channelName[0] != CHANNEL_PREFIX) {
        channelName = CHANNEL_PREFIX + channelName;
    }

    // チャンネルが存在するか確認
    if (!_server->channelExists(channelName)) {
        _client->sendNumericReply(ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }

    Channel* channel = _server->getChannel(channelName);

    // クライアントがチャンネルに参加しているか確認
    if (!channel->isClientInChannel(_client)) {
        _client->sendNumericReply(ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }

    // パラメータが1つだけの場合はトピックを表示
    if (_params.size() == 1) {
        if (channel->getTopic().empty()) {
            _client->sendNumericReply(RPL_NOTOPIC, channelName + " :No topic is set");
        } else {
            _client->sendNumericReply(RPL_TOPIC, channelName + " :" + channel->getTopic());
        }
        return;
    }

    // トピックを設定
    std::string newTopic = _params[1];

    // トピックが制限されている場合はオペレータ権限が必要
    if (channel->isTopicRestricted() && !channel->isOperator(_client->getNickname())) {
        _client->sendNumericReply(ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }

    // トピックを設定
    channel->setTopic(newTopic);

    // トピック変更メッセージをブロードキャスト
    std::string topicMessage = ":" + _client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
    channel->broadcastMessage(topicMessage);
}

// MODE コマンド
ModeCommand::ModeCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "MODE", params)
{
}

ModeCommand::~ModeCommand() {
    // 特に何もしない
}

void ModeCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }

    std::string targetName = _params[0];

    // チャンネルモード
    if (targetName[0] == CHANNEL_PREFIX) {
        // チャンネルが存在するか確認
        if (!_server->channelExists(targetName)) {
            _client->sendNumericReply(ERR_NOSUCHCHANNEL, targetName + " :No such channel");
            return;
        }

        Channel* channel = _server->getChannel(targetName);

        // パラメータが1つだけの場合はモードを表示
        if (_params.size() == 1) {
            _client->sendNumericReply(RPL_CHANNELMODEIS, targetName + " " + channel->getModes());
            return;
        }

        // クライアントがチャンネルに参加しているか確認
        if (!channel->isClientInChannel(_client)) {
            _client->sendNumericReply(ERR_NOTONCHANNEL, targetName + " :You're not on that channel");
            return;
        }

        // クライアントがチャンネルオペレータかどうか確認
        if (!channel->isOperator(_client->getNickname())) {
            _client->sendNumericReply(ERR_CHANOPRIVSNEEDED, targetName + " :You're not channel operator");
            return;
        }

        std::string modeString = _params[1];

        // モード文字列が空の場合はエラー
        if (modeString.empty()) {
            return;
        }

        bool isAddMode = true;
        size_t paramIndex = 2;

        // モード文字列を処理
        for (size_t i = 0; i < modeString.length(); i++) {
            char c = modeString[i];

            if (c == '+') {
                isAddMode = true;
            } else if (c == '-') {
                isAddMode = false;
            } else {
                std::string param = "";

                // パラメータが必要なモードの場合
                if ((c == 'k' && isAddMode) || c == 'o' || (c == 'l' && isAddMode)) {
                    if (paramIndex < _params.size()) {
                        param = _params[paramIndex++];
                    } else {
                        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
                        continue;
                    }
                }

                // モードを適用
                bool success = channel->applyMode(c, isAddMode, param);

                if (!success) {
                    _client->sendNumericReply(ERR_UNKNOWNMODE, std::string(1, c) + " :is unknown mode char to me");
                } else {
                    // モード変更メッセージをブロードキャスト
                    std::string modeSign = isAddMode ? "+" : "-";
                    std::string modeMessage = ":" + _client->getPrefix() + " MODE " + targetName + " " + modeSign + c;

                    if (!param.empty()) {
                        modeMessage += " " + param;
                    }

                    channel->broadcastMessage(modeMessage);
                }
            }
        }
    }
    // ユーザーモード（こちらは簡易実装）
    else {
        // 自分自身のニックネームのみ変更可能
        if (targetName != _client->getNickname()) {
            _client->sendNumericReply(ERR_USERSDONTMATCH, ":Cannot change mode for other users");
            return;
        }

        // パラメータが1つだけの場合は現在のモードを表示
        if (_params.size() == 1) {
            std::string modes = "+";
            if (_client->isOperator()) {
                modes += "o";
            }
            _client->sendNumericReply(RPL_UMODEIS, modes);
            return;
        }

        // ユーザーモードの変更は現在サポートしていない
        _client->sendNumericReply(ERR_UMODEUNKNOWNFLAG, ":Unknown MODE flag");
    }
}
