#include "../../include/Command.hpp"
#include "../../include/Server.hpp"

// JOIN コマンド
JoinCommand::JoinCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "JOIN", params)
{
}

JoinCommand::~JoinCommand() {
    // 特に何もしない
}

void JoinCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }

    // 特殊なケース: JOIN 0
    if (_params[0] == "0") {
        // すべてのチャンネルから退出
        std::vector<std::string> channels = _client->getChannels();
        for (std::vector<std::string>::iterator it = channels.begin(); it != channels.end(); ++it) {
            Channel* channel = _server->getChannel(*it);
            if (channel) {
                std::string partMessage = ":" + _client->getPrefix() + " PART " + channel->getName() + " :Left all channels";
                channel->broadcastMessage(partMessage);
                channel->removeClient(_client);
            }
        }
        return;
    }

    // チャンネル名とキーの取得
    std::vector<std::string> channelNames = Utils::split(_params[0], ',');
    std::vector<std::string> keys;

    if (_params.size() > 1) {
        keys = Utils::split(_params[1], ',');
    }

    // 複数のチャンネルに対して処理
    for (size_t i = 0; i < channelNames.size(); i++) {
        std::string channelName = channelNames[i];

        // チャンネル名の先頭に # がない場合は追加
        if (channelName[0] != CHANNEL_PREFIX) {
            channelName = CHANNEL_PREFIX + channelName;
        }

        // キーの取得
        std::string key = "";
        if (i < keys.size()) {
            key = keys[i];
        }

        // チャンネルが存在しない場合は作成
        if (!_server->channelExists(channelName)) {
            _server->createChannel(channelName, _client);

            // 参加メッセージ
            std::string joinMessage = ":" + _client->getPrefix() + " JOIN " + channelName;
            _client->sendMessage(joinMessage);

            Channel* channel = _server->getChannel(channelName);

            // トピックのレスポンス
            if (channel->getTopic().empty()) {
                _client->sendNumericReply(RPL_NOTOPIC, channelName + " :No topic is set");
            } else {
                _client->sendNumericReply(RPL_TOPIC, channelName + " :" + channel->getTopic());
            }

            // 参加者リストを送信
            channel->sendNames(_client);
        } else {
            // 既存のチャンネルに参加
            Channel* channel = _server->getChannel(channelName);

            // チャンネル参加処理
            bool joined = channel->addClient(_client, key);

            if (!joined) {
                // 参加失敗の理由を送信
                if (channel->hasKey() && (key.empty() || key != channel->getKey())) {
                    _client->sendNumericReply(ERR_BADCHANNELKEY, channelName + " :Cannot join channel (+k)");
                } else if (channel->isInviteOnly() && !channel->isInvited(_client->getNickname())) {
                    _client->sendNumericReply(ERR_INVITEONLYCHAN, channelName + " :Cannot join channel (+i)");
                } else if (channel->hasUserLimit() && channel->getClientCount() >= channel->getUserLimit()) {
                    _client->sendNumericReply(ERR_CHANNELISFULL, channelName + " :Cannot join channel (+l)");
                }
            } else {
                // 参加メッセージをブロードキャスト
                std::string joinMessage = ":" + _client->getPrefix() + " JOIN " + channelName;
                channel->broadcastMessage(joinMessage);

                // トピックのレスポンス
                if (channel->getTopic().empty()) {
                    _client->sendNumericReply(RPL_NOTOPIC, channelName + " :No topic is set");
                } else {
                    _client->sendNumericReply(RPL_TOPIC, channelName + " :" + channel->getTopic());
                }

                // 参加者リストを送信
                channel->sendNames(_client);
            }
        }
    }
}

// PART コマンド
PartCommand::PartCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "PART", params)
{
}

PartCommand::~PartCommand() {
    // 特に何もしない
}

void PartCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
        return;
    }

    // チャンネル名のリスト
    std::vector<std::string> channelNames = Utils::split(_params[0], ',');

    // 退出メッセージ（オプション）
    std::string partMessage = "";
    if (_params.size() > 1) {
        partMessage = _params[1];
    }

    // 各チャンネルから退出
    for (size_t i = 0; i < channelNames.size(); i++) {
        std::string channelName = channelNames[i];

        // チャンネル名の先頭に # がない場合は追加
        if (channelName[0] != CHANNEL_PREFIX) {
            channelName = CHANNEL_PREFIX + channelName;
        }

        // チャンネルが存在するか確認
        if (!_server->channelExists(channelName)) {
            _client->sendNumericReply(ERR_NOSUCHCHANNEL, channelName + " :No such channel");
            continue;
        }

        Channel* channel = _server->getChannel(channelName);

        // クライアントがチャンネルに参加しているか確認
        if (!channel->isClientInChannel(_client)) {
            _client->sendNumericReply(ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
            continue;
        }

        // 退出メッセージを作成
        std::string fullPartMessage = ":" + _client->getPrefix() + " PART " + channelName;
        if (!partMessage.empty()) {
            fullPartMessage += " :" + partMessage;
        }

        // 退出メッセージをブロードキャスト
        channel->broadcastMessage(fullPartMessage);

        // クライアントをチャンネルから削除
        channel->removeClient(_client);

        // チャンネルが空になった場合は削除
        if (channel->getClientCount() == 0) {
            _server->removeChannel(channelName);
        }
    }
}
