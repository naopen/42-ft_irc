#include "../include/Server.hpp"
#include "../include/Command.hpp"

Server::Server(int port, const std::string& password)
    : _serverSocket(-1), _password(password), _port(port), _running(false), _commandFactory(NULL), _detailedView(false)
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

    // 終了時に端末設定を元に戻す
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

void Server::setup() {
    initializeSocket();

    // SIGPIPEを無視
    signal(SIGPIPE, SIG_IGN);

    // 入力モードを非カノニカルに設定（キー入力を即時処理するため）
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    // 起動メッセージ
    std::cout << "\033[1;32m[SERVER] ft_irc server listening on port " << _port << "\033[0m" << std::endl;
    std::cout << "\033[1;37mPress 'd' to toggle detailed view mode\033[0m" << std::endl;

    // 初期ステータス表示
    displayServerStatus();

    // 起動ログ
    std::string startLog = "Server started on port " + Utils::toString(_port);
    addConnectionLog(startLog, "\033[1;32m");
}

void Server::run() {
    _running = true;

    while (_running) {
        // キー入力をチェック
        checkInput();

        // サーバーの状態を表示
        if (_detailedView) {
            displayDetailedStatus();
        } else {
            displayServerStatus();
        }

        // pollfdの更新
        updatePollFds();

        // poll関数でイベントを監視（短いタイムアウトを設定してキー入力をチェックできるようにする）
        int pollResult = poll(&_pollfds[0], _pollfds.size(), 100); // 100msのタイムアウト

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
                } else if (_pollfds[i].fd == STDIN_FILENO) {
                    // 標準入力からのキー入力（既にcheckInput()で処理済み）
                    continue;
                } else {
                    // クライアントからのデータを処理
                    handleClientData(i);
                }
            }

            // エラーや切断を処理
            if (_pollfds[i].revents & (POLLHUP | POLLERR)) {
                if (_pollfds[i].fd != _serverSocket && _pollfds[i].fd != STDIN_FILENO) {
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

    // ログメッセージ
    std::string connectionLog = "[+] New client connected: " + Utils::toString(fd) + " from " + hostname;
    std::cout << "\033[1;32m" << connectionLog << "\033[0m" << std::endl;

    // 接続履歴に追加
    addConnectionLog(connectionLog, "\033[1;32m");

    // ステータス更新
    displayServerStatus();
}

void Server::removeClient(int fd) {
    Client* client = getClientByFd(fd);
    if (client) {
        // ログメッセージ作成
        std::string disconnectLog = "[-] Client disconnected: " + Utils::toString(fd);
        if (!client->getNickname().empty()) {
            disconnectLog += " (" + client->getNickname() + ")";

            // ニックネームマップから削除 - 現在のニックネームを削除
            std::string nickname = client->getNickname();
            if (_nicknames.find(nickname) != _nicknames.end()) {
                std::cout << "\n\033[1;35m[NICKMAP] Removing nickname: " << nickname << "\033[0m" << std::endl;
                _nicknames.erase(nickname);
            }

            // ニックネームマップの整合性チェック - このクライアントが別のニックネームでマップに残っていないか確認
            std::vector<std::string> nicksToRemove;
            for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
                if (it->second == client) {
                    nicksToRemove.push_back(it->first);
                }
            }

            // 見つかった古いニックネームを削除
            for (std::vector<std::string>::iterator it = nicksToRemove.begin(); it != nicksToRemove.end(); ++it) {
                std::cout << "\033[1;35m[NICKMAP] Removing stale nickname: " << *it << "\033[0m" << std::endl;
                _nicknames.erase(*it);
            }
        }

        // ログを表示
        std::cout << "\033[1;31m" << disconnectLog << "\033[0m" << std::endl;

        // 接続履歴に追加
        addConnectionLog(disconnectLog, "\033[1;31m");

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

        // 状態表示を更新
        displayServerStatus();
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
        std::cout << "\033[1;35m[NICKMAP] Added new nickname: " << newNick << "\033[0m" << std::endl;

        // ログメッセージ
        std::string nickLog;
        if (!oldNick.empty()) {
            nickLog = "[*] Nickname changed: " + oldNick + " -> " + newNick;
        } else {
            nickLog = "[*] New nickname registered: " + newNick;
        }
        addConnectionLog(nickLog, "\033[1;36m");
    } else {
        std::cout << "\033[1;31m[ERROR] Failed to find client for nickname update\033[0m" << std::endl;
        return;
    }

    // デバッグ: 現在のニックネームマップを表示
    if (_detailedView) {
        std::cout << "\033[1;35m[NICKMAP] Current map:\033[0m" << std::endl;
        for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
            std::cout << "  " << it->first << " -> Client on fd " << it->second->getFd()
                      << " (actual nickname: " << it->second->getNickname() << ")" << std::endl;
        }
    }

    // マップの整合性チェック - 警告表示
    for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
        if (it->first != it->second->getNickname()) {
            std::cout << "\033[1;31m[WARNING] Nickname map inconsistency detected: "
                      << it->first << " != " << it->second->getNickname() << "\033[0m" << std::endl;

            // 接続履歴に警告を追加
            std::string warningLog = "[!] Nickname inconsistency: " + it->first + " != " + it->second->getNickname();
            addConnectionLog(warningLog, "\033[1;31m");
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
    return NULL;
}

void Server::createChannel(const std::string& name, Client* creator) {
    if (!channelExists(name)) {
        Channel* channel = new Channel(name, creator);
        _channels[name] = channel;

        // ログメッセージ
        std::string channelLog = "[+] Channel created: " + name + " by " + creator->getNickname();
        std::cout << "\033[1;33m" << channelLog << "\033[0m" << std::endl;

        // 接続履歴に追加
        addConnectionLog(channelLog, "\033[1;33m");

        // 状態表示を更新
        displayServerStatus();
    }
}

void Server::removeChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = _channels.find(name);
    if (it != _channels.end()) {
        // ログメッセージ
        std::string channelLog = "[-] Channel removed: " + name;
        std::cout << "\033[1;33m" << channelLog << "\033[0m" << std::endl;

        // 接続履歴に追加
        addConnectionLog(channelLog, "\033[1;33m");

        delete it->second;
        _channels.erase(it);

        // 状態表示を更新
        displayServerStatus();
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

    std::cout << "\033[1;36m[MSG] Received from " << client->getFd();
    if (!client->getNickname().empty()) {
        std::cout << " (" << client->getNickname() << ")";
    }
    std::cout << ": " << message << "\033[0m" << std::endl;

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

void Server::displayServerStatus() {
    static time_t lastUpdate = 0;
    time_t currentTime = time(NULL);

    // 1秒に1回だけ更新（画面のフラッシュを抑制）
    if (currentTime - lastUpdate < 1) {
        return;
    }

    lastUpdate = currentTime;

    // 端末制御シーケンスを使って画面を分割表示する
    // 画面をクリアせず、カーソルを先頭に移動
    // ANSI制御コードでカーソルを保存し、先頭に移動
    std::cout << "\033[s\033[H\033[K";

    // ===== 固定ステータス領域（上部） =====

    // サーバー情報を表示
    std::cout << "\033[1;44m=== ft_irc Server Status ===\033[0m\033[K" << std::endl;
    std::cout << "Hostname: " << _hostname << " | Port: " << _port << " | Uptime: "
              << (currentTime - _startTime) << " seconds\033[K" << std::endl;

    // 重要な概要情報
    std::cout << "\033[1;33mUsers: " << _clients.size()
              << " | Channels: " << _channels.size()
              << " | Nicknames: " << _nicknames.size()
              << " | Recent events:\033[0m\033[K" << std::endl;

    // 最近の接続・切断情報（最大5件）
    showRecentConnections();

    // サマリー情報（短く）
    std::cout << "\n\033[1;36m=== Summary ===\033[0m\033[K" << std::endl;

    // チャンネル一覧（短く）
    if (!_channels.empty()) {
        std::cout << "Channels: ";
        int count = 0;
        for (std::map<std::string, Channel*>::iterator it = _channels.begin();
             it != _channels.end() && count < 3; ++it, ++count) {
            std::cout << it->first << "(" << it->second->getClientCount() << ") ";
        }
        if (_channels.size() > 3) {
            std::cout << "... +" << (_channels.size() - 3) << " more";
        }
        std::cout << "\033[K" << std::endl;
    }

    // ユーザー一覧（短く）
    if (!_clients.empty()) {
        std::cout << "Users: ";
        int count = 0;
        for (std::map<int, Client*>::iterator it = _clients.begin();
             it != _clients.end() && count < 5; ++it, ++count) {
            if (!it->second->getNickname().empty()) {
                std::cout << it->second->getNickname() << " ";
            } else {
                std::cout << "fd(" << it->first << ") ";
            }
        }
        if (_clients.size() > 5) {
            std::cout << "... +" << (_clients.size() - 5) << " more";
        }
        std::cout << "\033[K" << std::endl;
    }

    // オペレーター情報（短く）
    std::cout << "Operators: ";
    int opCount = 0;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel* channel = it->second;
        std::vector<Client*> clients = channel->getClients();

        for (std::vector<Client*>::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
            if (channel->isOperator((*cit)->getNickname())) {
                std::cout << (*cit)->getNickname() << "@" << channel->getName() << " ";
                opCount++;
                if (opCount >= 3) {
                    break;
                }
            }
        }
        if (opCount >= 3) {
            break;
        }
    }
    if (opCount == 0) {
        std::cout << "None";
    }
    std::cout << "\033[K" << std::endl;

    // ニックネーム情報（新しい内容を追加）
    if (_nicknames.size() > 0) {
        std::cout << "Nicknames: ";
        int count = 0;

        // 最大5件まで表示
        for (std::map<std::string, Client*>::iterator it = _nicknames.begin();
             it != _nicknames.end() && count < 5; ++it, ++count) {

            // 不一致があれば警告表示
            if (it->first != it->second->getNickname()) {
                std::cout << "\033[1;31m" << it->first << "≠" << it->second->getNickname() << "\033[0m ";
            } else {
                std::cout << it->first << " ";
            }
        }

        if (_nicknames.size() > 5) {
            std::cout << "... +" << (_nicknames.size() - 5) << " more";
        }
        std::cout << "\033[K" << std::endl;
    }

    // コマンド情報行を追加
    std::cout << "\033[1;37m--- Log output below (press 'd' for detailed view) ---\033[0m\033[K" << std::endl;

    // カーソル位置を復元（ログ出力用）
    std::cout << "\033[u";
}

// 最近の接続・切断情報を表示するヘルパーメソッド
void Server::showRecentConnections() {
    // 静的変数として最近の接続情報を保持する
    static std::vector<std::string> recentConnections;
    static std::vector<std::string> recentColors;
    static std::vector<time_t> connectionTimes;

    // 最大表示数
    const size_t MAX_CONNECTIONS = 5;

    // 現在の接続情報を表示
    for (size_t i = 0; i < recentConnections.size() && i < MAX_CONNECTIONS; ++i) {
        // 接続から2分以上経過した場合はリストから削除
        if (time(NULL) - connectionTimes[i] > 120) {
            continue;
        }

        // 接続情報を表示
        std::cout << recentColors[i] << recentConnections[i] << "\033[0m\033[K" << std::endl;
    }

    // 必要な行数を埋める（常に一定の行数を確保）
    for (size_t i = recentConnections.size(); i < MAX_CONNECTIONS; ++i) {
        std::cout << "\033[K" << std::endl;
    }
}

// 接続情報をリストに追加するメソッド
void Server::addConnectionLog(const std::string& log, const std::string& color) {
    // 静的変数として最近の接続情報を保持する
    static std::vector<std::string> recentConnections;
    static std::vector<std::string> recentColors;
    static std::vector<time_t> connectionTimes;

    // 最大保持数
    const size_t MAX_CONNECTIONS = 10;

    // 古いログをチェックして削除（2分以上経過）
    for (size_t i = 0; i < connectionTimes.size(); ++i) {
        if (time(NULL) - connectionTimes[i] > 120) {
            recentConnections.erase(recentConnections.begin() + i);
            recentColors.erase(recentColors.begin() + i);
            connectionTimes.erase(connectionTimes.begin() + i);
            --i; // インデックスを調整
        }
    }

    // 新しいログを先頭に追加
    recentConnections.insert(recentConnections.begin(), log);
    recentColors.insert(recentColors.begin(), color);
    connectionTimes.insert(connectionTimes.begin(), time(NULL));

    // 最大数を超えたら古いものから削除
    if (recentConnections.size() > MAX_CONNECTIONS) {
        recentConnections.pop_back();
        recentColors.pop_back();
        connectionTimes.pop_back();
    }
}

// 詳細表示モードを切り替えるメソッド
void Server::toggleDetailedView() {
    _detailedView = !_detailedView;

    // モード切替メッセージ
    std::string modeMessage = "Switched to " + std::string(_detailedView ? "detailed" : "normal") + " view mode";
    addConnectionLog(modeMessage, "\033[1;35m");

    // 画面クリア
    std::cout << "\033[2J\033[H";
}

// 詳細ステータス表示
void Server::displayDetailedStatus() {
    static time_t lastUpdate = 0;
    time_t currentTime = time(NULL);

    // 1秒に1回だけ更新（画面のフラッシュを抑制）
    if (currentTime - lastUpdate < 1) {
        return;
    }

    lastUpdate = currentTime;

    // 画面クリア＆カーソル先頭
    std::cout << "\033[2J\033[H";

    // サーバー情報を表示
    std::cout << "\033[1;44m=== ft_irc Server Status (DETAILED VIEW - press 'd' to return) ===\033[0m" << std::endl;
    std::cout << "Hostname: " << _hostname << " | Port: " << _port << " | Uptime: "
              << (currentTime - _startTime) << " seconds" << std::endl;

    // チャンネル情報
    std::cout << "\033[1;44m=== Channels (" << _channels.size() << ") ===\033[0m" << std::endl;
    if (_channels.empty()) {
        std::cout << "  No channels" << std::endl;
    } else {
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
            Channel* channel = it->second;
            std::cout << "  " << channel->getName() << " (" << channel->getClientCount() << " users)";

            // チャンネルモードを表示
            std::cout << " [" << channel->getModes() << "]";

            // トピックを表示
            if (!channel->getTopic().empty()) {
                std::cout << " Topic: " << channel->getTopic();
            }

            std::cout << std::endl;
        }
    }

    // ユーザー情報
    std::cout << "\033[1;44m=== Connected Users (" << _clients.size() << ") ===\033[0m" << std::endl;
    if (_clients.empty()) {
        std::cout << "  No users connected" << std::endl;
    } else {
        std::cout << "  FD | Nickname | Username | Status | Channels | Operator" << std::endl;
        std::cout << "  ------------------------------------------------------" << std::endl;

        for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            Client* client = it->second;
            std::string status;

            switch (client->getStatus()) {
                case CONNECTING:
                    status = "Connecting";
                    break;
                case REGISTERING:
                    status = "Registering";
                    break;
                case REGISTERED:
                    status = "Registered";
                    break;
                default:
                    status = "Unknown";
            }

            // オペレーターかどうか
            std::string opStatus = client->isOperator() ? "Yes" : "No";

            // 参加中のチャンネル数
            size_t channelCount = client->getChannels().size();

            std::cout << "  " << client->getFd() << " | "
                      << (client->getNickname().empty() ? "*" : client->getNickname()) << " | "
                      << (client->getUsername().empty() ? "*" : client->getUsername()) << " | "
                      << status << " | "
                      << channelCount << " | "
                      << opStatus << std::endl;
        }
    }

    // チャンネルオペレーター情報
    std::cout << "\033[1;44m=== Channel Operators ===\033[0m" << std::endl;
    bool hasOperators = false;

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        Channel* channel = it->second;
        std::vector<Client*> clients = channel->getClients();
        std::vector<std::string> operators;

        // チャンネルのオペレーターを収集
        for (std::vector<Client*>::iterator cit = clients.begin(); cit != clients.end(); ++cit) {
            if (channel->isOperator((*cit)->getNickname())) {
                operators.push_back((*cit)->getNickname());
            }
        }

        if (!operators.empty()) {
            hasOperators = true;
            std::cout << "  " << channel->getName() << ": ";

            for (size_t i = 0; i < operators.size(); ++i) {
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << operators[i];
            }

            std::cout << std::endl;
        }
    }

    if (!hasOperators) {
        std::cout << "  No channel operators" << std::endl;
    }

    // ニックネームマップの情報も表示
    std::cout << "\033[1;44m=== Nickname Map ===\033[0m" << std::endl;
    if (_nicknames.empty()) {
        std::cout << "  No nicknames registered" << std::endl;
    } else {
        std::cout << "  Nickname | Client FD | Actual Nickname" << std::endl;
        std::cout << "  --------------------------------------" << std::endl;

        for (std::map<std::string, Client*>::iterator it = _nicknames.begin(); it != _nicknames.end(); ++it) {
            std::cout << "  " << it->first << " | " << it->second->getFd() << " | " << it->second->getNickname();

            // マップの整合性チェック - 不一致があれば警告表示
            if (it->first != it->second->getNickname()) {
                std::cout << " \033[1;31m[MISMATCH]\033[0m";
            }

            std::cout << std::endl;
        }
    }

    // 最近の接続情報
    std::cout << "\033[1;44m=== Recent Connections ===\033[0m" << std::endl;
    // 静的変数として最近の接続情報を参照
    static std::vector<std::string> recentConnections;
    static std::vector<std::string> recentColors;

    if (recentConnections.empty()) {
        std::cout << "  No recent connections" << std::endl;
    } else {
        for (size_t i = 0; i < recentConnections.size() && i < 10; ++i) {
            std::cout << "  " << recentColors[i] << recentConnections[i] << "\033[0m" << std::endl;
        }
    }

    std::cout << "\033[1;44m===========================\033[0m" << std::endl;
}

// キー入力をチェックするメソッド
void Server::checkInput() {
    char c;

    // 標準入力から1文字読み込む（ノンブロッキング）
    if (read(STDIN_FILENO, &c, 1) > 0) {
        // 'd'キーが押されたら詳細表示モードを切り替え
        if (c == 'd' || c == 'D') {
            toggleDetailedView();
        }
    }
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
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        exit(EXIT_FAILURE);
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        perror("fcntl F_SETFL");
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

    // 標準入力を追加（キー入力用）
    struct pollfd stdinPollFd;
    stdinPollFd.fd = STDIN_FILENO;
    stdinPollFd.events = POLLIN;
    stdinPollFd.revents = 0;
    _pollfds.push_back(stdinPollFd);

    // クライアントソケットを追加
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        struct pollfd clientPollFd;
        clientPollFd.fd = it->first;
        clientPollFd.events = POLLIN;
        clientPollFd.revents = 0;
        _pollfds.push_back(clientPollFd);
    }
}