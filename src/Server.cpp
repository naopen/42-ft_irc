#include "../include/Server.hpp"
#include "../include/Command.hpp"
#include "../include/bonus/BotManager.hpp"
#include "../include/DCCManager.hpp"

Server::Server(int port, const std::string& password)
    : _serverSocket(-1), _password(password), _port(port), _running(false), _commandFactory(NULL), _botManager(NULL), _dccManager(NULL)
{
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        _hostname = hostname;
    } else {
        _hostname = "localhost";
    }

    _startTime = time(NULL);
    _commandFactory = new CommandFactory(this);
    _botManager = new BotManager(this);
    _dccManager = new DCCManager(this);
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
    
    // BotManagerの解放
    if (_botManager) {
        delete _botManager;
        _botManager = NULL;
    }
    
    // DCCManagerの解放
    if (_dccManager) {
        delete _dccManager;
        _dccManager = NULL;
    }
}

void Server::setup() {
    initializeSocket();

    // SIGPIPEを無視
    signal(SIGPIPE, SIG_IGN);
    
    // Botを初期化
    if (_botManager) {
        _botManager->initializeBots();
    }

    std::cout << "\033[1;32m[SERVER] ft_irc server listening on port " << _port << "\033[0m" << std::endl;

    // 初期ステータス表示
    displayServerStatus();
}

void Server::run() {
    _running = true;

    // 状態変化を追跡するための変数を初期化
    size_t lastClientCount = _clients.size();
    size_t lastChannelCount = _channels.size();
    size_t lastNicknameCount = _nicknames.size();
    time_t lastDisplayTime = time(NULL); // 最後に表示した時間を記録

    while (_running) {
        // 現在の時間を取得
        time_t currentTime = time(NULL);

        // クライアント数、チャンネル数、ニックネーム数のいずれかが変わった場合にのみステータスを更新
        // かつ、最後の表示から少なくとも1秒経過している場合のみ表示する
        if (((_clients.size() != lastClientCount ||
             _channels.size() != lastChannelCount ||
             _nicknames.size() != lastNicknameCount) &&
             (currentTime - lastDisplayTime >= 1)))
        {
            // 空のチャンネルをチェックして削除（ステータス表示前に）
            checkAndRemoveEmptyChannels();

            displayServerStatus();
            // 現在の状態を保存
            lastClientCount = _clients.size();
            lastChannelCount = _channels.size();
            lastNicknameCount = _nicknames.size();
            lastDisplayTime = currentTime;
        }

        // pollfdの更新
        updatePollFds();

        // poll関数でイベントを監視（例外処理を追加）
        int pollResult = 0;
        try {
            pollResult = poll(&_pollfds[0], _pollfds.size(), 1000); // 1秒のタイムアウト
        } catch (const std::exception& e) {
            std::cerr << "\033[1;31m[ERROR] Exception in poll(): " << e.what() << "\033[0m" << std::endl;
            continue;
        }

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
            // 安全チェック：無効なfdをスキップ
            if (_pollfds[i].fd < 0) {
                continue;
            }

            if (_pollfds[i].revents & POLLIN) {
                if (_pollfds[i].fd == _serverSocket) {
                    // 新しい接続を処理
                    handleNewConnection();
                } else {
                    // クライアントが存在するかチェック
                    if (!getClientByFd(_pollfds[i].fd)) {
                        std::cout << "\033[1;31m[WARNING] Skipping invalid client fd: " << _pollfds[i].fd << "\033[0m" << std::endl;
                        // 無効なfdを_pollfdから削除
                        removePollFd(_pollfds[i].fd);
                        // インデックスを調整
                        i--;
                        continue;
                    }
                    // クライアントからのデータを処理
                    handleClientData(i);
                }
            }

            // エラーや切断を処理
            if (_pollfds[i].revents & (POLLHUP | POLLERR)) {
                if (_pollfds[i].fd != _serverSocket) {
                    // クライアントが存在するかチェック
                    if (!getClientByFd(_pollfds[i].fd)) {
                        std::cout << "\033[1;31m[WARNING] Skipping invalid client fd: " << _pollfds[i].fd << "\033[0m" << std::endl;
                        // 無効なfdを_pollfdから削除
                        removePollFd(_pollfds[i].fd);
                        // インデックスを調整
                        i--;
                        continue;
                    }
                    // クライアントを削除
                    removeClient(_pollfds[i].fd);
                    // インデックスを調整（削除したので）
                    i--;
                }
            }
        }

        // 切断されたクライアントをチェック
        checkDisconnectedClients();

        // 空のチャンネルをチェックして削除
        checkAndRemoveEmptyChannels();
        
        // DCC転送を処理
        if (_dccManager) {
            _dccManager->processTransfers();
        }
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

    std::cout << "\033[1;32m[+] New client connected: " << fd << " from " << hostname << "\033[0m" << std::endl;
    displayServerStatus();
}

void Server::removeClient(int fd) {
    Client* client = getClientByFd(fd);
    if (client) {
        std::cout << "\033[1;31m[-] Client disconnected: " << fd;
        if (!client->getNickname().empty()) {
            std::cout << " (" << client->getNickname() << ")";

            // ニックネームマップから削除
            std::string nickname = client->getNickname();
            if (_nicknames.find(nickname) != _nicknames.end()) {
                std::cout << "\n\033[1;35m[NICKMAP] Removing nickname: " << nickname << "\033[0m";
                _nicknames.erase(nickname);
            }

            // 古いニックネーム削除処理
            std::vector<std::string> nicksToRemove;
            for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
                if (it->second == client) {
                    nicksToRemove.push_back(it->first);
                }
            }

            for (std::vector<std::string>::iterator it = nicksToRemove.begin(); it != nicksToRemove.end(); ++it) {
                std::cout << "\n\033[1;35m[NICKMAP] Removing stale nickname: " << *it << "\033[0m";
                _nicknames.erase(*it);
            }
        }
        std::cout << "\033[0m" << std::endl;

        // DCC転送をクリーンアップ
        if (_dccManager) {
            _dccManager->removeClientTransfers(client);
        }
        
        // チャンネルからクライアントを削除（クライアント削除前にコピー）
        std::vector<std::string> channels = client->getChannels();
        for (std::vector<std::string>::iterator it = channels.begin(); it != channels.end(); ++it) {
            Channel* channel = getChannel(*it);
            if (channel) {
                std::cout << "\033[1;33m[CHANNEL] Removing client " << client->getNickname() << " from channel " << *it << "\033[0m" << std::endl;
                channel->removeClient(client);
                // ここではチャンネル削除を行わない
            }
        }

        // クライアントの削除
        delete client;
        _clients.erase(fd);

        // pollFDの削除
        removePollFd(fd);

        // チャンネル削除処理（一括で行う）
        checkAndRemoveEmptyChannels();

        // 状態表示更新
        displayServerStatus();
    }
}

void Server::removeClient(const std::string& nickname) {
    Client* client = getClientByNickname(nickname);
    if (client) {
        removeClient(client->getFd());
    } else {
        std::cout << "\033[1;31m[ERROR] Cannot remove client with nickname " << nickname << ": not found in nickname map\033[0m" << std::endl;
    }
}

bool Server::isNicknameInUse(const std::string& nickname) {
    bool inUse = _nicknames.find(nickname) != _nicknames.end();
    std::cout << "\033[1;36m[NICKMAP] Checking if nickname '" << nickname << "' is in use: " << (inUse ? "YES" : "NO") << "\033[0m" << std::endl;
    return inUse;
}

void Server::updateNickname(const std::string& oldNick, const std::string& newNick) {
    std::cout << "\033[1;35m[NICKMAP] Updating: '" << oldNick << "' -> '" << newNick << "'\033[0m" << std::endl;

    // クライアントを特定
    Client* client = NULL;

    // 古いニックネームがある場合はそれを使ってクライアントを特定
    if (!oldNick.empty()) {
        std::map<std::string, Client*>::iterator it = _nicknames.find(oldNick);
        if (it != _nicknames.end()) {
            client = it->second;

            // 古いニックネームをマップから削除
            std::cout << "\033[1;35m[NICKMAP] Removing old nickname: " << oldNick << "\033[0m" << std::endl;
            _nicknames.erase(it);
        } else {
            std::cout << "\033[1;31m[ERROR] Old nickname not found in map: " << oldNick << "\033[0m" << std::endl;
        }
    }

    // クライアントが特定できなかった場合、FDマップから探す
    if (!client) {
        for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->getNickname() == newNick) {
                client = it->second;
                std::cout << "\033[1;35m[NICKMAP] Client found by new nickname in client map\033[0m" << std::endl;
                break;
            }
        }
    }

    // クライアントが見つかった場合、新しいニックネームで登録
    if (client) {
        _nicknames[newNick] = client;
        std::cout << "\033[1;35m[NICKMAP] Added new nickname: " << newNick << " for client on fd " << client->getFd() << "\033[0m" << std::endl;
    } else {
        std::cout << "\033[1;31m[ERROR] Failed to find client for nickname update\033[0m" << std::endl;
        return;
    }

    // デバッグ: 現在のニックネームマップを表示
    std::cout << "\033[1;35m[NICKMAP] Current map (" << _nicknames.size() << " entries):\033[0m" << std::endl;
    for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
        std::cout << "  " << it->first << " -> Client on fd " << it->second->getFd()
                  << " (actual nickname: " << it->second->getNickname() << ")" << std::endl;
    }

    // マップの整合性チェック - 警告表示
    for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
        if (it->first != it->second->getNickname()) {
            std::cout << "\033[1;31m[WARNING] Nickname map inconsistency detected: "
                      << it->first << " != " << it->second->getNickname() << "\033[0m" << std::endl;
        }
    }

    // 状態表示を更新
    displayServerStatus();
}

Channel* Server::getChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        return it->second;
    }
    std::cout << "\033[1;33m[CHANNEL] Channel not found: " << name << "\033[0m" << std::endl;
    return NULL;
}

void Server::createChannel(const std::string& name, Client* creator) {
    if (!channelExists(name)) {
        Channel* channel = new Channel(name, creator);
        _channels[name] = channel;

        std::cout << "\033[1;33m[+] Channel created: " << name << " by " << creator->getNickname() << "\033[0m" << std::endl;

        // オペレーター設定のログ
        std::cout << "\033[1;33m[CHANNEL] Setting " << creator->getNickname() << " as operator for " << name << "\033[0m" << std::endl;

        displayServerStatus();
    } else {
        std::cout << "\033[1;33m[CHANNEL] Cannot create channel " << name << ": already exists\033[0m" << std::endl;
    }
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        std::cout << "\033[1;33m[-] Channel removed: " << name << "\033[0m" << std::endl;

        delete it->second;
        _channels.erase(it);

        // 状態表示を更新
        displayServerStatus();
    } else {
        std::cout << "\033[1;33m[CHANNEL] Cannot remove channel " << name << ": not found\033[0m" << std::endl;
    }
}

bool Server::channelExists(const std::string& name) const {
    bool exists = _channels.find(name) != _channels.end();
    std::cout << "\033[1;33m[CHANNEL] Checking if channel '" << name << "' exists: " << (exists ? "YES" : "NO") << "\033[0m" << std::endl;
    return exists;
}

std::map<std::string, Channel*>& Server::getChannels() {
    return _channels;
}

void Server::processClientMessage(int fd) {
    Client* client = getClientByFd(fd);
    if (!client) {
        std::cout << "\033[1;31m[ERROR] Cannot process message from fd " << fd << ": client not found\033[0m" << std::endl;
        return;
    }

    char buffer[BUFFER_SIZE + 1];
    ssize_t bytesRead;

    // データを読み込む
    bytesRead = recv(fd, buffer, BUFFER_SIZE, 0);

    if (bytesRead < 0) {
        // エラー
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "\033[1;31m[ERROR] recv() failed for fd " << fd << ": " << strerror(errno) << "\033[0m" << std::endl;
            perror("recv");
            removeClient(fd);
        }
        return;
    } else if (bytesRead == 0) {
        // クライアントが正常に切断（または複数回Ctrl+D）
        std::cout << "\033[1;31m[CLIENT] Connection closed by client on fd " << fd << "\033[0m" << std::endl;
        removeClient(fd);
        return;
    }

    // 受信したデータをバッファに追加
    buffer[bytesRead] = '\0';
    std::cout << "\033[1;36m[CLIENT] Received " << bytesRead << " bytes from fd " << fd << "\033[0m" << std::endl;
    client->appendToBuffer(buffer);

    // 完全なメッセージを処理
    std::vector<std::string> messages = client->getCompleteMessages();
    std::cout << "\033[1;36m[CLIENT] Extracted " << messages.size() << " complete messages from buffer\033[0m" << std::endl;

    for (std::vector<std::string>::iterator it = messages.begin(); it != messages.end(); ++it) {
        executeCommand(client, *it);
    }
}

void Server::executeCommand(Client* client, const std::string& message) {
    if (message.empty()) {
        std::cout << "\033[1;31m[COMMAND] Empty message received, ignoring\033[0m" << std::endl;
        return;
    }

    std::cout << "\033[1;36m[MSG] Received from " << client->getFd();
    if (!client->getNickname().empty()) {
        std::cout << " (" << client->getNickname() << ")";
    }
    std::cout << ": " << message << "\033[0m" << std::endl;

    // コマンドを作成して実行
    Command* command = _commandFactory->createCommand(client, message);
    if (command) {
        std::cout << "\033[1;36m[COMMAND] Executing command: " << command->getName() << "\033[0m" << std::endl;
        command->execute();
        delete command;
    } else {
        std::cout << "\033[1;31m[COMMAND] Failed to create command for message: " << message << "\033[0m" << std::endl;
    }
}

bool Server::authenticateClient(Client* client, const std::string& password) {
    bool authenticated = _password == password;
    std::cout << "\033[1;35m[AUTH] Client " << client->getFd() << " authentication: "
              << (authenticated ? "SUCCESS" : "FAILED") << "\033[0m" << std::endl;
    return authenticated;
}

bool Server::checkPassword(const std::string& password) const {
    bool correct = _password == password;
    std::cout << "\033[1;35m[AUTH] Password check: " << (correct ? "CORRECT" : "INCORRECT") << "\033[0m" << std::endl;
    return correct;
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

void Server::displayServerStatus() {
    // 現在表示中のステータス内容を生成
    std::stringstream statusStream;

    // 区切り線
    statusStream << "\033[1;44m";
    for (int i = 0; i < 50; i++) statusStream << "=";
    statusStream << "\033[0m" << std::endl;

    // サーバー情報
    statusStream << "\033[1;32m=== ft_irc Server Status ===\033[0m" << std::endl;
    statusStream << "Hostname: " << _hostname << " | Port: " << _port << " | Uptime: "
              << (time(NULL) - _startTime) << " seconds" << std::endl;

    // ユーザー情報
    statusStream << "\033[1;36m=== Connected Users (" << _clients.size() << ") ===\033[0m" << std::endl;
    if (_clients.empty()) {
        statusStream << "No users connected" << std::endl;
    } else {
        // 最大表示人数
        int maxUsers = 10;
        int count = 0;

        for (std::map<int, Client*>::iterator it = _clients.begin();
             it != _clients.end() && count < maxUsers; ++it, ++count) {
            Client* client = it->second;

            statusStream << "• " << client->getFd() << ": ";
            // ニックネーム情報を追加
            if (!client->getNickname().empty()) {
                statusStream << client->getNickname();
            } else {
                statusStream << "(no nickname)";
            }
            if (!client->getUsername().empty()) {
                statusStream << " [" << client->getUsername() << "]";
            } else {
                statusStream << " [no username]";
            }

            // ステータス表示
            switch (client->getStatus()) {
                case CONNECTING: statusStream << " [connecting]"; break;
                case REGISTERING: statusStream << " [registering]"; break;
                case REGISTERED: statusStream << " [registered]"; break;
                default: statusStream << " [unknown]";
            }

            // チャンネル数
            if (!client->getChannels().empty()) {
                statusStream << " in " << client->getChannels().size() << " channels";
            }

            statusStream << std::endl;
        }

        if (_clients.size() > (size_t)maxUsers) {
            statusStream << "... and " << (_clients.size() - maxUsers) << " more users" << std::endl;
        }
    }

    // チャンネル情報
    statusStream << "\033[1;33m=== Channels (" << _channels.size() << ") ===\033[0m" << std::endl;
    if (_channels.empty()) {
        statusStream << "No channels" << std::endl;
    } else {
        // 最大表示数
        int maxChannels = 10;
        int count = 0;

        for (std::map<std::string, Channel*>::iterator it = _channels.begin();
             it != _channels.end() && count < maxChannels; ++it, ++count) {
            Channel* channel = it->second;

            // クライアント数をリアルタイムに取得
            size_t clientCount = channel->getClientCount();

            statusStream << "• " << channel->getName() << " (" << clientCount << " users)";

            // オペレーター表示（最大3人）
            std::vector<Client*> clients = channel->getClients();
            std::vector<std::string> operators;

            for (std::vector<Client*>::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
                if (channel->isOperator((*cit)->getNickname())) {
                    operators.push_back((*cit)->getNickname());
                }
            }

            if (!operators.empty()) {
                statusStream << " [ops: ";
                for (size_t i = 0; i < operators.size() && i < 3; ++i) {
                    if (i > 0) statusStream << ", ";
                    statusStream << operators[i];
                }

                if (operators.size() > 3) {
                    statusStream << ", +" << (operators.size() - 3) << " more";
                }

                statusStream << "]";
            }

            statusStream << std::endl;
        }

        if (_channels.size() > (size_t)maxChannels) {
            statusStream << "... and " << (_channels.size() - maxChannels) << " more channels" << std::endl;
        }
    }

    // ニックネームマップ情報（問題があれば表示）
    int inconsistencies = 0;
    for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
        if (it->first != it->second->getNickname()) {
            inconsistencies++;
        }
    }

    if (inconsistencies > 0) {
        statusStream << "\033[1;31m=== Nickname Map Issues (" << inconsistencies << ") ===\033[0m" << std::endl;

        for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
            if (it->first != it->second->getNickname()) {
                statusStream << "• Map entry '" << it->first << "' points to client with nickname '"
                          << it->second->getNickname() << "'" << std::endl;
            }
        }
    }

    // ニックネームマップ情報（常時表示）
    statusStream << "\033[1;35m=== Nickname Map (" << _nicknames.size() << ") ===\033[0m" << std::endl;
    if (_nicknames.empty()) {
        statusStream << "No registered nicknames" << std::endl;
    } else {
        // 最大表示数
        int maxNicks = 10;
        int count = 0;

        for (std::map<std::string, Client*>::iterator it = _nicknames.begin();
             it != _nicknames.end() && count < maxNicks; ++it, ++count) {

            statusStream << "• " << it->first << " -> fd:" << it->second->getFd();

            // 不整合があれば強調表示
            if (it->first != it->second->getNickname()) {
                statusStream << " \033[1;31m[MISMATCH: actual=" << it->second->getNickname() << "]\033[0m";
            }

            statusStream << std::endl;
        }

        if (_nicknames.size() > (size_t)maxNicks) {
            statusStream << "... and " << (_nicknames.size() - maxNicks) << " more nicknames" << std::endl;
        }
    }

    // 区切り線
    statusStream << "\033[1;44m";
    for (int i = 0; i < 50; i++) statusStream << "=";
    statusStream << "\033[0m" << std::endl;

    // 一度にステータスを表示（画面のちらつきを防止）
    std::cout << statusStream.str();
}

void Server::initializeSocket() {
    struct sockaddr_in serverAddr;

    // ポート番号の範囲チェック（冗長チェック）
    if (_port <= 0 || _port > 65535) {
        std::cerr << "\033[1;31m[ERROR] Invalid port number: " << _port << "\033[0m" << std::endl;
        exit(EXIT_FAILURE);
    }

    // ソケットの作成
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket < 0) {
        std::cout << "\033[1;31m[ERROR] socket() failed: " << strerror(errno) << "\033[0m" << std::endl;
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDRオプションの設定
    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cout << "\033[1;31m[ERROR] setsockopt() failed: " << strerror(errno) << "\033[0m" << std::endl;
        perror("setsockopt");
        close(_serverSocket);
        exit(EXIT_FAILURE);
    }

    // // SO_REUSEPORTオプションの設定（可能な場合）
    // #ifdef SO_REUSEPORT
    // if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
    //     std::cout << "\033[1;33m[WARNING] setsockopt(SO_REUSEPORT) failed: " << strerror(errno) << "\033[0m" << std::endl;
    //     // 致命的でないためcontinue
    // }
    // #endif

    // // TCP_NODELAYオプションの設定（パフォーマンス向上）
    // #ifdef TCP_NODELAY
    // if (setsockopt(_serverSocket, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
    //     std::cout << "\033[1;33m[WARNING] setsockopt(TCP_NODELAY) failed: " << strerror(errno) << "\033[0m" << std::endl;
    //     // 致命的でないためcontinue
    // }
    // #endif

    // ノンブロッキングに設定
    setNonBlocking(_serverSocket);

    // サーバーアドレスの設定
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(_port);

    // ソケットをアドレスにバインド
    if (bind(_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cout << "\033[1;31m[ERROR] bind() failed: " << strerror(errno) << "\033[0m" << std::endl;
        perror("bind");
        close(_serverSocket);
        exit(EXIT_FAILURE);
    }

    // リスニングの開始
    if (listen(_serverSocket, 10) < 0) {
        std::cout << "\033[1;31m[ERROR] listen() failed: " << strerror(errno) << "\033[0m" << std::endl;
        perror("listen");
        close(_serverSocket);
        exit(EXIT_FAILURE);
    }

    std::cout << "\033[1;32m[SERVER] Successfully initialized socket on port " << _port << "\033[0m" << std::endl;
}

void Server::setNonBlocking(int fd) {
    // fcntlを使用してノンブロッキングに設定
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        std::cout << "\033[1;31m[ERROR] fcntl(F_GETFL) failed for fd " << fd << ": " << strerror(errno) << "\033[0m" << std::endl;
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        std::cout << "\033[1;31m[ERROR] fcntl(F_SETFL) failed for fd " << fd << ": " << strerror(errno) << "\033[0m" << std::endl;
        perror("fcntl F_SETFL");
        exit(EXIT_FAILURE);
    }

    std::cout << "\033[1;32m[SERVER] Set fd " << fd << " to non-blocking mode\033[0m" << std::endl;
}

void Server::handleNewConnection() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket;

    // 新しい接続を受け入れる
    clientSocket = accept(_serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

    if (clientSocket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "\033[1;31m[ERROR] accept() failed: " << strerror(errno) << "\033[0m" << std::endl;
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

    std::cout << "\033[1;32m[SERVER] New connection from " << hostBuffer << " on socket " << clientSocket << "\033[0m" << std::endl;

    // クライアントを追加
    addClient(clientSocket, hostBuffer);
}

void Server::handleClientData(size_t i) {
    int clientFd = _pollfds[i].fd;

    std::cout << "\033[1;36m[SERVER] Activity detected on fd " << clientFd << "\033[0m" << std::endl;

    processClientMessage(clientFd);
}

void Server::checkDisconnectedClients() {
    // タイムアウトしたクライアントを削除
    // （この実装では行わない）
}

void Server::checkAndRemoveEmptyChannels() {
    std::vector<std::string> channelsToRemove;

    // 最初のパス：削除するチャンネルを特定する
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel* channel = it->second;
        std::vector<Client*> clients = channel->getClients();

        // クライアント数が0の場合は削除対象
        if (clients.empty()) {
            channelsToRemove.push_back(it->first);
            std::cout << "\033[1;33m[CLEANUP] Marking empty channel for removal: " << it->first << "\033[0m" << std::endl;
        } else {
            // クライアントが実際に有効かどうかをチェック
            bool validClientsExist = false;

            for (std::vector<Client*>::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
                // クライアントが有効かどうかをチェック（_clientsマップに存在するか）
                if (*cit != NULL && getClientByFd((*cit)->getFd()) != NULL) {
                    validClientsExist = true;
                    break;
                }
            }

            // 有効なクライアントが一人もいない場合も削除対象
            if (!validClientsExist) {
                channelsToRemove.push_back(it->first);
                std::cout << "\033[1;33m[CLEANUP] Marking channel with invalid clients for removal: " << it->first
                         << " (client count: " << clients.size() << ")\033[0m" << std::endl;
            }
        }
    }

    // 第二パス：特定したチャンネルを安全に削除する
    for (std::vector<std::string>::iterator it = channelsToRemove.begin(); it != channelsToRemove.end(); ++it) {
        std::string channelName = *it;
        std::cout << "\033[1;33m[CLEANUP] Removing empty channel: " << channelName << "\033[0m" << std::endl;

        // マップ内にまだチャンネルが存在するかどうかをチェック
        std::map<std::string, Channel*>::iterator channelIt = _channels.find(channelName);
        if (channelIt != _channels.end()) {
            Channel* channel = channelIt->second;

            // マップから先に削除し、それからチャンネルオブジェクトを削除
            _channels.erase(channelIt);

            std::cout << "\033[1;33m[-] Channel removed: " << channelName << "\033[0m" << std::endl;
            delete channel;
        }
    }
}

void Server::updatePollFds() {
    // 前回の poll FDs の数を保存
    static size_t lastPollFDCount = 0;

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

    // FD の数が変わった場合のみログを出力
    if (_pollfds.size() != lastPollFDCount) {
        std::cout << "\033[1;36m[SERVER] Poll array updated: " << _pollfds.size() << " file descriptors monitored\033[0m" << std::endl;
        lastPollFDCount = _pollfds.size();
    }
}

void Server::removePollFd(int fd) {
    for (std::vector<struct pollfd>::iterator it = _pollfds.begin(); it != _pollfds.end(); ++it) {
        if (it->fd == fd) {
            _pollfds.erase(it);
            std::cout << "\033[1;36m[SERVER] Removed fd " << fd << " from poll array\033[0m" << std::endl;
            break;
        }
    }
}

BotManager* Server::getBotManager() {
    return _botManager;
}

DCCManager* Server::getDCCManager() {
    return _dccManager;
}
