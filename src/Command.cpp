#include "../include/Command.hpp"
#include "../include/Server.hpp"
#include "../include/DCCCommand.hpp"

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
        std::cout << "\033[1;31m[COMMAND] " << _name << " failed: client not registered\033[0m" << std::endl;
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
    // メッセージのサイズチェック
    if (message.empty()) {
        std::cout << "\033[1;31m[COMMAND] Empty message received\033[0m" << std::endl;
        return NULL;
    }

    if (message.length() > 512) {
        std::cout << "\033[1;31m[COMMAND] Message too long, truncating to 512 characters\033[0m" << std::endl;
        // メッセージが長すぎる場合は無視（RFC準拠）
        return NULL;
    }

    Parser parser(message);
    if (!parser.isValid()) {
        std::cout << "\033[1;31m[PARSER] Invalid message format: " << message << "\033[0m" << std::endl;
        return NULL;
    }

    std::string command = parser.getCommand();
    std::vector<std::string> params = parser.getParams();

    // 不正なコマンド名のチェック
    if (command.empty() || command.length() > 16) {
        std::cout << "\033[1;31m[COMMAND] Invalid command name: '" << command << "'\033[0m" << std::endl;
        return NULL;
    }

    // 過剰なパラメータ数のチェック
    if (params.size() > 15) {
        std::cout << "\033[1;31m[COMMAND] Too many parameters: " << params.size() << " (max 15)\033[0m" << std::endl;
        // RFC 2812によれば、最大15パラメータまで
        params.resize(15);
    }

    std::cout << "\033[1;36m[COMMAND] Creating command: " << command;
    if (!params.empty()) {
        std::cout << " with params:";
        for (size_t i = 0; i < params.size() && i < 3; ++i) {
            std::cout << " [" << params[i] << "]";
        }
        if (params.size() > 3) {
            std::cout << " ...";
        }
    }
    std::cout << "\033[0m" << std::endl;

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
    } else if (command == "DCC") {
        // DCCサブコマンドの処理
        if (params.empty()) {
            client->sendMessage(":server NOTICE " + client->getNickname() + 
                              " :Usage: DCC <SEND|GET|ACCEPT|REJECT|LIST|CANCEL|STATUS> ...\r\n");
            return NULL;
        }
        
        std::string subCommand = params[0];
        // パラメータからサブコマンドを削除
        std::vector<std::string> dccParams(params.begin() + 1, params.end());
        
        if (subCommand == "SEND") {
            return new DCCSendCommand(_server, client, dccParams);
        } else if (subCommand == "GET" || subCommand == "ACCEPT") {
            return new DCCGetCommand(_server, client, dccParams);
        } else if (subCommand == "REJECT") {
            return new DCCRejectCommand(_server, client, dccParams);
        } else if (subCommand == "LIST") {
            return new DCCListCommand(_server, client, dccParams);
        } else if (subCommand == "CANCEL") {
            return new DCCCancelCommand(_server, client, dccParams);
        } else if (subCommand == "STATUS") {
            return new DCCStatusCommand(_server, client, dccParams);
        } else {
            client->sendMessage(":server NOTICE " + client->getNickname() + 
                              " :Unknown DCC subcommand: " + subCommand + "\r\n");
            return NULL;
        }
    }

    // 未知のコマンド
    std::cout << "\033[1;31m[COMMAND] Unknown command: " << command << "\033[0m" << std::endl;

    // クライアントがすでに登録されている場合のみエラーを送信
    if (client->isRegistered()) {
        client->sendNumericReply(421, command + " :Unknown command");
    }

    return NULL;
    }
