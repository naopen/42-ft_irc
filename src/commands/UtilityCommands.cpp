#include "../../include/Command.hpp"
#include "../../include/Server.hpp"

// PING コマンド
PingCommand::PingCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "PING", params)
{
    _requiresRegistration = false;
}

PingCommand::~PingCommand() {
    // 特に何もしない
}

void PingCommand::execute() {
    // パラメータがない場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NOORIGIN, ":No origin specified");
        return;
    }

    // PONG レスポンスを送信
    std::string pongMessage = ":" + _server->getHostname() + " PONG " + _server->getHostname() + " :" + _params[0];
    _client->sendMessage(pongMessage);
}

// PONG コマンド
PongCommand::PongCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "PONG", params)
{
    _requiresRegistration = false;
}

PongCommand::~PongCommand() {
    // 特に何もしない
}

void PongCommand::execute() {
    // PONGコマンドは特に処理しない
    // クライアントの最終アクティビティ時間は既に更新されている
}

// QUIT コマンド
QuitCommand::QuitCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "QUIT", params)
{
    _requiresRegistration = false;
}

QuitCommand::~QuitCommand() {
    // 特に何もしない
}

void QuitCommand::execute() {
    // 退出メッセージ
    std::string quitMessage = "Quit";
    if (!_params.empty()) {
        quitMessage = _params[0];
    }

    // 参加しているすべてのチャンネルに退出メッセージを送信
    std::vector<std::string> channels = _client->getChannels();
    for (std::vector<std::string>::iterator it = channels.begin(); it != channels.end(); ++it) {
        Channel* channel = _server->getChannel(*it);
        if (channel) {
            std::string message = ":" + _client->getPrefix() + " QUIT :" + quitMessage;
            channel->broadcastMessage(message, _client);
        }
    }

    // クライアントを削除（サーバーのマップからも削除される）
    _server->removeClient(_client->getFd());
}

// WHO コマンド
WhoCommand::WhoCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "WHO", params)
{
}

WhoCommand::~WhoCommand() {
    // 特に何もしない
}

void WhoCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータがない場合、または一致する名前がない場合は空のレスポンスを返す
    if (_params.empty()) {
        _client->sendNumericReply(315, "* :End of WHO list");
        return;
    }

    std::string mask = _params[0];

    // チャンネル名の場合
    if (mask[0] == CHANNEL_PREFIX) {
        if (_server->channelExists(mask)) {
            Channel* channel = _server->getChannel(mask);
            std::vector<Client*> clients = channel->getClients();

            for (std::vector<Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
                Client* c = *it;
                std::string flags = "H"; // Here (not away)

                if (c->isAway()) {
                    flags = "G"; // Gone (away)
                }

                if (channel->isOperator(c->getNickname())) {
                    flags += "@"; // Channel operator
                }

                // <channel> <user> <host> <server> <nick> <H|G>[*][@|+] :<hopcount> <real name>
                _client->sendNumericReply(352, mask + " " + c->getUsername() + " " + c->getHostname() + " " +
                                         _server->getHostname() + " " + c->getNickname() + " " + flags + " :0 " + c->getRealname());
            }
        }

        _client->sendNumericReply(315, mask + " :End of WHO list");
    }
    // ユーザー名/ニックネームの場合
    else {
        std::map<std::string, Channel*>& channels = _server->getChannels();

        for (std::map<std::string, Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            Channel* channel = it->second;
            std::vector<Client*> clients = channel->getClients();

            for (std::vector<Client*>::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
                Client* c = *cit;

                // マスクに一致するかチェック
                if (c->getNickname() == mask || c->getUsername() == mask) {
                    std::string flags = "H"; // Here (not away)

                    if (c->isAway()) {
                        flags = "G"; // Gone (away)
                    }

                    if (channel->isOperator(c->getNickname())) {
                        flags += "@"; // Channel operator
                    }

                    // <channel> <user> <host> <server> <nick> <H|G>[*][@|+] :<hopcount> <real name>
                    _client->sendNumericReply(352, channel->getName() + " " + c->getUsername() + " " + c->getHostname() + " " +
                                             _server->getHostname() + " " + c->getNickname() + " " + flags + " :0 " + c->getRealname());

                    break; // 同じユーザーを複数回表示しないようにする
                }
            }
        }

        _client->sendNumericReply(315, mask + " :End of WHO list");
    }
}

// WHOIS コマンド
WhoisCommand::WhoisCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "WHOIS", params)
{
}

WhoisCommand::~WhoisCommand() {
    // 特に何もしない
}

void WhoisCommand::execute() {
    if (!canExecute()) {
        return;
    }

    // パラメータがない場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(431, ":No nickname given");
        return;
    }

    std::string targetNick = _params[0];

    // ターゲットユーザーが存在するか確認
    Client* targetClient = _server->getClientByNickname(targetNick);
    if (!targetClient) {
        _client->sendNumericReply(401, targetNick + " :No such nick/channel");
        _client->sendNumericReply(318, targetNick + " :End of /WHOIS list");
        return;
    }

    // ユーザー情報を送信
    _client->sendNumericReply(311, targetNick + " " + targetClient->getUsername() + " " +
                             targetClient->getHostname() + " * :" + targetClient->getRealname());

    // ユーザーが参加しているチャンネル情報を送信
    std::string channels = "";
    std::vector<std::string> userChannels = targetClient->getChannels();

    for (std::vector<std::string>::iterator it = userChannels.begin(); it != userChannels.end(); ++it) {
        Channel* channel = _server->getChannel(*it);
        if (channel) {
            if (channel->isOperator(targetNick)) {
                channels += "@" + channel->getName() + " ";
            } else {
                channels += channel->getName() + " ";
            }
        }
    }

    if (!channels.empty()) {
        _client->sendNumericReply(319, targetNick + " :" + channels);
    }

    // サーバー情報を送信
    _client->sendNumericReply(312, targetNick + " " + _server->getHostname() + " :ft_irc server");

    // 離席情報を送信
    if (targetClient->isAway()) {
        _client->sendNumericReply(301, targetNick + " :" + targetClient->getAwayMessage());
    }

    // WHOIS終了
    _client->sendNumericReply(318, targetNick + " :End of /WHOIS list");
}

// CAP コマンド
CapCommand::CapCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "CAP", params)
{
    _requiresRegistration = false;
}

CapCommand::~CapCommand() {
    // 特に何もしない
}

void CapCommand::execute() {
    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        return;
    }

    std::string subcommand = Utils::toUpper(_params[0]);

    if (subcommand == "LS") {
        // CAP LS 要求に対して、サポートされている機能を返す
        // 現在のサーバーでは特別な機能をサポートしていないため空リストを返す
        std::string response = ":" + _server->getHostname() + " CAP " +
                              (_client->getNickname().empty() ? "*" : _client->getNickname()) +
                              " LS :";
        _client->sendMessage(response);
    } else if (subcommand == "LIST") {
        // 現在アクティブな機能のリストを返す（現状では空）
        std::string response = ":" + _server->getHostname() + " CAP " +
                              (_client->getNickname().empty() ? "*" : _client->getNickname()) +
                              " LIST :";
        _client->sendMessage(response);
    } else if (subcommand == "REQ") {
        // クライアントが機能を要求
        // 現在の実装ではどの機能もサポートしていないのでNAKを返す
        if (_params.size() > 1) {
            std::string response = ":" + _server->getHostname() + " CAP " +
                                  (_client->getNickname().empty() ? "*" : _client->getNickname()) +
                                  " NAK :" + _params[1];
            _client->sendMessage(response);
        }
    } else if (subcommand == "END") {
        // CAP ネゴシエーションの終了
        // 特に何もしない、クライアントは通常の登録処理に進む
    }
}