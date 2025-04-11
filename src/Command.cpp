#include "../include/Command.hpp"
#include "../include/Server.hpp"

// コマンドベースクラス
Command::Command(Server* server, Client* client, const std::string& name, const std::vector<std::string>& params)
    : _server(server), _client(client), _name(name), _params(params), _requiresRegistration(true)
{
}

Command::~Command() {
    // 特に何もしない
}

bool Command::requiresRegistration() const {
    return _requiresRegistration;
}

bool Command::canExecute() const {
    if (_requiresRegistration && !_client->isRegistered()) {
        _client->sendNumericReply(ERR_NOTREGISTERED, ":You have not registered");
        return false;
    }
    return true;
}

std::string Command::getName() const {
    return _name;
}

Client* Command::getClient() const {
    return _client;
}

Server* Command::getServer() const {
    return _server;
}

std::vector<std::string> Command::getParams() const {
    return _params;
}

// コマンドファクトリークラス
CommandFactory::CommandFactory(Server* server) : _server(server) {
}

CommandFactory::~CommandFactory() {
    // 特に何もしない
}

Command* CommandFactory::createCommand(Client* client, const std::string& message) {
    Parser parser(message);
    if (!parser.isValid()) {
        return NULL;
    }

    std::string command = parser.getCommand();
    std::vector<std::string> params = parser.getParams();

    // 使用可能なコマンドを作成
    if (command == "PASS") {
        return new PassCommand(_server, client, params);
    } else if (command == "NICK") {
        return new NickCommand(_server, client, params);
    } else if (command == "USER") {
        return new UserCommand(_server, client, params);
    } else if (command == "QUIT") {
        return new QuitCommand(_server, client, params);
    } else if (command == "JOIN") {
        return new JoinCommand(_server, client, params);
    } else if (command == "PART") {
        return new PartCommand(_server, client, params);
    } else if (command == "PRIVMSG") {
        return new PrivmsgCommand(_server, client, params);
    } else if (command == "NOTICE") {
        return new NoticeCommand(_server, client, params);
    } else if (command == "KICK") {
        return new KickCommand(_server, client, params);
    } else if (command == "INVITE") {
        return new InviteCommand(_server, client, params);
    } else if (command == "TOPIC") {
        return new TopicCommand(_server, client, params);
    } else if (command == "MODE") {
        return new ModeCommand(_server, client, params);
    } else if (command == "PING") {
        return new PingCommand(_server, client, params);
    } else if (command == "PONG") {
        return new PongCommand(_server, client, params);
    } else if (command == "WHO") {
        return new WhoCommand(_server, client, params);
    } else if (command == "WHOIS") {
        return new WhoisCommand(_server, client, params);
    } else if (command == "CAP") {
        return new CapCommand(_server, client, params);
    }

    // 未知のコマンド
    // クライアントがすでに登録されている場合のみエラーを送信
    if (client->isRegistered()) {
        client->sendNumericReply(421, command + " :Unknown command");
    }

    return NULL;
}
