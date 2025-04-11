#include "../include/Server.hpp"
#include "../include/Command.hpp"

Server::Server(int port, const std::string& password)
    : _serverSocket(-1), _password(password), _port(port), _running(false), _commandFactory(NULL)
{
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        _hostname = hostname;
    } else {
        _hostname = "localhost";
    }

    _startTime = time(NULL);
    _commandFactory = new CommandFactory(this);
}

Server::~Server() {
    stop();

    // クライアントの解放
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
    }
    _clients.clear();

    // チャンネルの解放
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        delete it->second;
    }
    _channels.clear();

    // コマンドファクトリーの解放
    if (_commandFactory) {
        delete _commandFactory;
        _commandFactory = NULL;
    }
}

void Server::setup() {
    initializeSocket();

    // SIGPIPEを無視
    signal(SIGPIPE, SIG_IGN);

    std::cout << "Server listening on port " << _port << std::endl;
}

void Server::run() {
    _running = true;

    while (_running) {
        // pollfdの更新
        updatePollFds();

        // poll関数でイベントを監視
        int pollResult = poll(&_pollfds[0], _pollfds.size(), -1);

        if (pollResult < 0) {
            if (errno == EINTR) {
                // シグナル割り込みは無視
                continue;
            }
            perror("poll");
            break;
        }

        // 各ファイルディスクリプタのイベントを処理
        for (size_t i = 0; i < _pollfds.size(); i++) {
            if (_pollfds[i].revents & POLLIN) {
                if (_pollfds[i].fd == _serverSocket) {
                    // 新しい接続を処理
                    handleNewConnection();
                } else {
                    // クライアントからのデータを処理
                    handleClientData(i);
                }
            }

            // エラーや切断を処理
            if (_pollfds[i].revents & (POLLHUP | POLLERR)) {
                if (_pollfds[i].fd != _serverSocket) {
                    // クライアントを削除
                    removeClient(_pollfds[i].fd);
                }
            }
        }

        // 切断されたクライアントをチェック
        checkDisconnectedClients();
    }
}

void Server::stop() {
    _running = false;

    if (_serverSocket >= 0) {
        close(_serverSocket);
        _serverSocket = -1;
    }
}

Client* Server::getClientByFd(int fd) {
    std::map<int, Client*>::iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        return it->second;
    }
    return NULL;
}

Client* Server::getClientByNickname(const std::string& nickname) {
    std::map<std::string, Client*>::iterator it = _nicknames.find(nickname);
    if (it != _nicknames.end()) {
        return it->second;
    }
    return NULL;
}

void Server::addClient(int fd, const std::string& hostname) {
    Client* client = new Client(fd, hostname);
    _clients[fd] = client;

    // ファイルディスクリプタをノンブロッキングに設定
    setNonBlocking(fd);

    std::cout << "New client connected: " << fd << " from " << hostname << std::endl;
}

void Server::removeClient(int fd) {
    Client* client = getClientByFd(fd);
    if (client) {
        std::cout << "Client disconnected: " << fd;
        if (!client->getNickname().empty()) {
            std::cout << " (" << client->getNickname() << ")";

            // ニックネームマップから削除
            _nicknames.erase(client->getNickname());
        }
        std::cout << std::endl;

        // すべてのチャンネルからクライアントを削除
        std::vector<std::string> channels = client->getChannels();
        for (std::vector<std::string>::iterator it = channels.begin(); it != channels.end(); ++it) {
            Channel* channel = getChannel(*it);
            if (channel) {
                channel->removeClient(client);

                // チャンネルが空になった場合は削除
                if (channel->getClientCount() == 0) {
                    removeChannel(*it);
                }
            }
        }

        // クライアントを削除
        delete client;
        _clients.erase(fd);
    }
}

void Server::removeClient(const std::string& nickname) {
    Client* client = getClientByNickname(nickname);
    if (client) {
        removeClient(client->getFd());
    }
}

bool Server::isNicknameInUse(const std::string& nickname) {
    return _nicknames.find(nickname) != _nicknames.end();
}

void Server::updateNickname(const std::string& oldNick, const std::string& newNick) {
    std::cout << "Updating nickname map: '" << oldNick << "' -> '" << newNick << "'" << std::endl;

    if (!oldNick.empty()) {
        std::map<std::string, Client*>::iterator it = _nicknames.find(oldNick);
        if (it != _nicknames.end()) {
            Client* client = it->second;
            _nicknames.erase(it);
            _nicknames[newNick] = client;
            std::cout << "Nickname map updated successfully" << std::endl;
        }
    } else {
        // 新しいニックネームをクライアントから取得して登録
        for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->getNickname() == newNick) {
                _nicknames[newNick] = it->second;
                std::cout << "New nickname added to map: " << newNick << std::endl;
                break;
            }
        }
    }

    // デバッグ: 現在のニックネームマップを表示
    std::cout << "Current nickname map:" << std::endl;
    for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
        std::cout << "  " << it->first << " -> Client on fd " << it->second->getFd() << std::endl;
    }
}

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        return it->second;
    }
    return NULL;
}

void Server::createChannel(const std::string& name, Client* creator) {
    if (!channelExists(name)) {
        Channel* channel = new Channel(name, creator);
        _channels[name] = channel;

        std::cout << "Channel created: " << name << " by " << creator->getNickname() << std::endl;
    }
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        std::cout << "Channel removed: " << name << std::endl;

        delete it->second;
        _channels.erase(it);
    }
}

bool Server::channelExists(const std::string& name) const {
    return _channels.find(name) != _channels.end();
}

std::map<std::string, Channel*>& Server::getChannels() {
    return _channels;
}

void Server::processClientMessage(int fd) {
    Client* client = getClientByFd(fd);
    if (!client) {
        return;
    }

    char buffer[BUFFER_SIZE + 1];
    ssize_t bytesRead;

    // データを読み込む
    bytesRead = recv(fd, buffer, BUFFER_SIZE, 0);

    if (bytesRead < 0) {
        // エラー
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recv");
            removeClient(fd);
        }
        return;
    } else if (bytesRead == 0) {
        // クライアントが正常に切断（または複数回Ctrl+D）
        removeClient(fd);
        return;
    }

    // 受信したデータをバッファに追加
    buffer[bytesRead] = '\0';
    client->appendToBuffer(buffer);

    // 完全なメッセージを処理
    std::vector<std::string> messages = client->getCompleteMessages();
    for (std::vector<std::string>::iterator it = messages.begin(); it != messages.end(); ++it) {
        executeCommand(client, *it);
    }
}

void Server::executeCommand(Client* client, const std::string& message) {
    if (message.empty()) {
        return;
    }

    std::cout << "Received from " << client->getFd();
    if (!client->getNickname().empty()) {
        std::cout << " (" << client->getNickname() << ")";
    }
    std::cout << ": " << message << std::endl;

    // コマンドを作成して実行
    Command* command = _commandFactory->createCommand(client, message);
    if (command) {
        command->execute();
        delete command;
    }
}

bool Server::authenticateClient(Client*, const std::string& password) {
    return _password == password;
}

bool Server::checkPassword(const std::string& password) const {
    return _password == password;
}

std::string Server::getHostname() const {
    return _hostname;
}

std::string Server::getPassword() const {
    return _password;
}

int Server::getPort() const {
    return _port;
}

time_t Server::getStartTime() const {
    return _startTime;
}

void Server::initializeSocket() {
    struct sockaddr_in serverAddr;

    // ソケットの作成
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDRオプションの設定
    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // ノンブロッキングに設定
    setNonBlocking(_serverSocket);

    // サーバーアドレスの設定
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(_port);

    // ソケットをアドレスにバインド
    if (bind(_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // リスニングの開始
    if (listen(_serverSocket, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}

void Server::setNonBlocking(int fd) {
    // fcntlを使用してノンブロッキングに設定
    int flags = fcntl(fd, F_SETFL, O_NONBLOCK);
    if (flags < 0) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
}

void Server::handleNewConnection() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket;

    // 新しい接続を受け入れる
    clientSocket = accept(_serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

    if (clientSocket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept");
        }
        return;
    }

    // クライアントのホスト名を取得
    char hostBuffer[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&clientAddr, clientAddrLen, hostBuffer, sizeof(hostBuffer), NULL, 0, NI_NUMERICHOST) != 0) {
        // ホスト名が取得できない場合は IP アドレスを使用
        strncpy(hostBuffer, inet_ntoa(clientAddr.sin_addr), sizeof(hostBuffer) - 1);
        hostBuffer[sizeof(hostBuffer) - 1] = '\0';
    }

    // クライアントを追加
    addClient(clientSocket, hostBuffer);
}

void Server::handleClientData(size_t i) {
    int clientFd = _pollfds[i].fd;
    processClientMessage(clientFd);
}

void Server::checkDisconnectedClients() {
    // タイムアウトしたクライアントを削除
    // （この実装では行わない）
}

void Server::updatePollFds() {
    _pollfds.clear();

    // サーバーソケットを追加
    struct pollfd serverPollFd;
    serverPollFd.fd = _serverSocket;
    serverPollFd.events = POLLIN;
    serverPollFd.revents = 0;
    _pollfds.push_back(serverPollFd);

    // クライアントソケットを追加
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        struct pollfd clientPollFd;
        clientPollFd.fd = it->first;
        clientPollFd.events = POLLIN;
        clientPollFd.revents = 0;
        _pollfds.push_back(clientPollFd);
    }
}