#ifndef COMMAND_HPP
# define COMMAND_HPP

# include "Utils.hpp"
# include "Client.hpp"
# include "Channel.hpp"
# include "Server.hpp"

class Server;

// コマンドベースクラス
class Command {
protected:
    Server*         _server;
    Client*         _client;
    std::string     _name;
    std::vector<std::string> _params;
    bool            _requiresRegistration;

public:
    Command(Server* server, Client* client, const std::string& name, const std::vector<std::string>& params);
    virtual ~Command();

    virtual void execute() = 0;

    // ヘルパーメソッド
    bool requiresRegistration() const;
    bool canExecute() const;
    std::string getName() const;
    Client* getClient() const;
    Server* getServer() const;
    std::vector<std::string> getParams() const;
};

// コマンドファクトリークラス
class CommandFactory {
private:
    Server* _server;

public:
    CommandFactory(Server* server);
    ~CommandFactory();

    Command* createCommand(Client* client, const std::string& message);
};

// 個別コマンドクラス
class PassCommand : public Command {
public:
    PassCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~PassCommand();

    void execute();
};

class NickCommand : public Command {
public:
    NickCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~NickCommand();

    void execute();
};

class UserCommand : public Command {
public:
    UserCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~UserCommand();

    void execute();
};

class QuitCommand : public Command {
public:
    QuitCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~QuitCommand();

    void execute();
};

class JoinCommand : public Command {
public:
    JoinCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~JoinCommand();

    void execute();
};

class PartCommand : public Command {
public:
    PartCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~PartCommand();

    void execute();
};

class PrivmsgCommand : public Command {
public:
    PrivmsgCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~PrivmsgCommand();

    void execute();
};

class NoticeCommand : public Command {
public:
    NoticeCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~NoticeCommand();

    void execute();
};

class KickCommand : public Command {
public:
    KickCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~KickCommand();

    void execute();
};

class InviteCommand : public Command {
public:
    InviteCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~InviteCommand();

    void execute();
};

class TopicCommand : public Command {
public:
    TopicCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~TopicCommand();

    void execute();
};

class ModeCommand : public Command {
public:
    ModeCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~ModeCommand();

    void execute();
};

class PingCommand : public Command {
public:
    PingCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~PingCommand();

    void execute();
};

class PongCommand : public Command {
public:
    PongCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~PongCommand();

    void execute();
};

class WhoCommand : public Command {
public:
    WhoCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~WhoCommand();

    void execute();
};

class WhoisCommand : public Command {
public:
    WhoisCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~WhoisCommand();

    void execute();
};

class CapCommand : public Command {
public:
    CapCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~CapCommand();

    void execute();
};

#endif