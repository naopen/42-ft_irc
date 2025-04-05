#ifndef SERVER_HPP
# define SERVER_HPP

# include "Utils.hpp"
# include "Client.hpp"
# include "Channel.hpp"
# include "Parser.hpp"

class Command;
class CommandFactory;

class Server {
private:
    int                                 _serverSocket;       // サーバーのリスニングソケット
    std::string                         _password;           // 接続パスワード
    std::string                         _hostname;           // サーバーホスト名
    int                                 _port;               // リスニングポート
    std::map<int, Client*>              _clients;            // クライアントマップ (fd -> Client*)
    std::map<std::string, Channel*>     _channels;           // チャンネルマップ (name -> Channel*)
    std::map<std::string, Client*>      _nicknames;          // ニックネームマップ (nickname -> Client*)
    std::vector<pollfd>                 _pollfds;            // poll用のfd配列
    bool                                _running;            // サーバー実行中フラグ
    CommandFactory*                     _commandFactory;     // コマンドファクトリー
    time_t                              _startTime;          // サーバー起動時間

public:
    Server(int port, const std::string& password);
    ~Server();

    // サーバー起動/停止メソッド
    void            setup();
    void            run();
    void            stop();

    // クライアント管理
    Client*         getClientByFd(int fd);
    Client*         getClientByNickname(const std::string& nickname);
    void            addClient(int fd, const std::string& hostname);
    void            removeClient(int fd);
    void            removeClient(const std::string& nickname);
    bool            isNicknameInUse(const std::string& nickname);
    void            updateNickname(const std::string& oldNick, const std::string& newNick);

    // チャンネル管理
    Channel*        getChannel(const std::string& name);
    void            createChannel(const std::string& name, Client* creator);
    void            removeChannel(const std::string& name);
    bool            channelExists(const std::string& name) const;
    std::map<std::string, Channel*>& getChannels();

    // メッセージ処理
    void            processClientMessage(int fd);
    void            executeCommand(Client* client, const std::string& message);

    // 接続管理
    bool            authenticateClient(Client* client, const std::string& password);
    bool            checkPassword(const std::string& password) const;

    // ゲッター
    std::string     getHostname() const;
    std::string     getPassword() const;
    int             getPort() const;
    time_t          getStartTime() const;

private:
    // ソケット初期化
    void            initializeSocket();
    void            setNonBlocking(int fd);
    void            handleNewConnection();
    void            handleClientData(size_t i);
    void            checkDisconnectedClients();
    void            updatePollFds();
};

#endif
