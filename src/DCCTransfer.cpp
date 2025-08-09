#include "../include/DCCTransfer.hpp"
#include "../include/Client.hpp"
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <cstring>

DCCTransfer::DCCTransfer(Client* sender, Client* receiver, const std::string& filename, 
                         unsigned long filesize, DCCTransferType type)
    : _sender(sender), _receiver(receiver), _filename(filename), _filesize(filesize),
      _bytesTransferred(0), _type(type), _status(DCC_PENDING), _listenSocket(-1),
      _dataSocket(-1), _port(0), _sendFile(NULL), _recvFile(NULL), _buffer(NULL),
      _lastFlushBytes(0) {
    
    _id = generateTransferId();
    _startTime = time(NULL);
    _lastActivity = _startTime;
    _buffer = new char[DCC_BUFFER_SIZE];
    
    // ファイルパスの設定
    if (_type == DCC_SEND) {
        // 送信側：ファイル名が与えられるので、パスを構築
        _filepath = "./dcc_transfers/" + _filename;
    } else {
        // 受信側：受信ディレクトリに保存
        _filepath = "./dcc_transfers/received/" + _filename;
    }
}

DCCTransfer::~DCCTransfer() {
    cleanup();
    if (_buffer) {
        delete[] _buffer;
        _buffer = NULL;
    }
}

bool DCCTransfer::initializeSend() {
    if (_type != DCC_SEND) {
        std::cout << "[DCC] initializeSend: Wrong type (not DCC_SEND)" << std::endl;
        return false;
    }
    
    // ファイルの存在確認
    std::cout << "[DCC] Checking file: " << _filepath << std::endl;
    if (!validateFilepath(_filepath)) {
        std::cout << "[DCC] File validation failed: " << _filepath << std::endl;
        _status = DCC_FAILED;
        return false;
    }
    
    // 送信用ファイルを開く
    if (!openSendFile()) {
        std::cout << "[DCC] Failed to open send file: " << _filepath << std::endl;
        _status = DCC_FAILED;
        return false;
    }
    
    // リスニングソケットの作成
    _listenSocket = createListenSocket();
    if (_listenSocket < 0) {
        std::cout << "[DCC] Failed to create listen socket" << std::endl;
        closeSendFile();
        _status = DCC_FAILED;
        return false;
    }
    
    // 送信者IPの取得
    _senderIP = getLocalIP();
    std::cout << "[DCC] Send initialized - IP: " << _senderIP << ", Port: " << _port << std::endl;
    
    return true;
}

bool DCCTransfer::initializeReceive(const std::string& ip, int port) {
    _senderIP = ip;
    _port = port;
    
    std::cout << "[DCC] Receive init - Connecting to " << _senderIP << ":" << _port << std::endl;
    
    // 受信用ファイルを開く
    if (!openReceiveFile()) {
        std::cout << "[DCC] Failed to open receive file: " << _filepath << std::endl;
        _status = DCC_FAILED;
        return false;
    }
    
    // 送信者に接続
    _dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_dataSocket < 0) {
        std::cout << "[DCC] Failed to create socket" << std::endl;
        closeReceiveFile();
        _status = DCC_FAILED;
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = inet_addr(_senderIP.c_str());
    
    std::cout << "[DCC] Connecting to " << _senderIP << ":" << _port << "..." << std::endl;
    if (connect(_dataSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "[DCC] Connection failed: " << strerror(errno) << std::endl;
        close(_dataSocket);
        _dataSocket = -1;
        closeReceiveFile();
        _status = DCC_FAILED;
        return false;
    }
    
    setSocketNonBlocking(_dataSocket);
    _status = DCC_ACTIVE;
    std::cout << "[DCC] Connection established successfully" << std::endl;
    
    return true;
}

bool DCCTransfer::acceptConnection() {
    if (_listenSocket < 0 || _type != DCC_SEND) {
        std::cout << "[DCC] acceptConnection: Invalid state (listenSocket=" << _listenSocket << ", type=" << _type << ")" << std::endl;
        return false;
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    _dataSocket = accept(_listenSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (_dataSocket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "[DCC] Accept failed: " << strerror(errno) << std::endl;
            _status = DCC_FAILED;
            return false;
        }
        // まだ接続がない（ノンブロッキング）
        return false;
    }
    
    std::cout << "[DCC] Connection accepted on socket " << _dataSocket << std::endl;
    setSocketNonBlocking(_dataSocket);
    
    // リスニングソケットを閉じる
    close(_listenSocket);
    _listenSocket = -1;
    
    _status = DCC_ACTIVE;
    updateLastActivity();
    std::cout << "[DCC] Transfer is now ACTIVE" << std::endl;
    
    return true;
}

bool DCCTransfer::sendData() {
    if (_status != DCC_ACTIVE || !_sendFile || _dataSocket < 0) {
        std::cout << "[DCC] sendData: Invalid state (status=" << _status << ", sendFile=" << (_sendFile ? "OK" : "NULL") << ", dataSocket=" << _dataSocket << ")" << std::endl;
        return false;
    }
    
    if (_sendFile->eof() || _bytesTransferred >= _filesize) {
        std::cout << "[DCC] Transfer complete: " << _bytesTransferred << "/" << _filesize << " bytes" << std::endl;
        _status = DCC_COMPLETED;
        return true;
    }
    
    _sendFile->read(_buffer, DCC_BUFFER_SIZE);
    std::streamsize bytesRead = _sendFile->gcount();
    
    if (bytesRead > 0) {
        ssize_t bytesSent = send(_dataSocket, _buffer, bytesRead, MSG_NOSIGNAL);
        
        if (bytesSent > 0) {
            _bytesTransferred += bytesSent;
            updateLastActivity();
            
            if (_bytesTransferred >= _filesize) {
                std::cout << "[DCC] Transfer complete: " << _bytesTransferred << "/" << _filesize << " bytes" << std::endl;
                _status = DCC_COMPLETED;
            }
            return true;
        } else if (bytesSent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cout << "[DCC] Send error: " << strerror(errno) << std::endl;
            _status = DCC_FAILED;
            return false;
        }
    }
    
    return true;
}

bool DCCTransfer::receiveData() {
    if (_status != DCC_ACTIVE || !_recvFile || _dataSocket < 0) {
        return false;
    }
    
    if (_bytesTransferred >= _filesize) {
        _status = DCC_COMPLETED;
        return true;
    }
    
    ssize_t bytesReceived = recv(_dataSocket, _buffer, DCC_BUFFER_SIZE, 0);
    
    if (bytesReceived > 0) {
        _recvFile->write(_buffer, bytesReceived);
        _bytesTransferred += bytesReceived;
        updateLastActivity();
        
        // 定期的なフラッシュ（64KB毎）または毎回フラッシュ
        if ((_bytesTransferred - _lastFlushBytes) >= DCC_FLUSH_INTERVAL || 
            _bytesTransferred % 32768 == 0) { // 32KB毎にもフラッシュ
            _recvFile->flush();
            _lastFlushBytes = _bytesTransferred;
        } else {
            // 小さなチャンクでも毎回フラッシュ（Linux環境での信頼性向上）
            _recvFile->flush();
        }
        
        // 受信確認の送信（DCC プロトコル）
        uint32_t ack = htonl(_bytesTransferred);
        send(_dataSocket, &ack, sizeof(ack), MSG_NOSIGNAL);
        
        if (_bytesTransferred >= _filesize) {
            _status = DCC_COMPLETED;
            // 転送完了時には確実にバッファをフラッシュ
            _recvFile->flush();
        }
        return true;
    } else if (bytesReceived == 0) {
        // 接続が閉じられた
        if (_bytesTransferred >= _filesize) {
            _status = DCC_COMPLETED;
        } else {
            _status = DCC_FAILED;
        }
        // 接続終了時にもバッファをフラッシュ
        if (_recvFile) {
            _recvFile->flush();
        }
        return false;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        _status = DCC_FAILED;
        return false;
    }
    
    return true;
}

bool DCCTransfer::processTransfer() {
    if (_status == DCC_PENDING && _type == DCC_SEND) {
        // 送信側：接続を待つ
        return acceptConnection();
    } else if (_status == DCC_ACTIVE) {
        if (_type == DCC_SEND) {
            // 送信側：データを送信
            return sendData();
        } else {
            // 受信側：データを受信
            return receiveData();
        }
    }
    return false;
}

void DCCTransfer::setStatus(DCCTransferStatus status) {
    _status = status;
    updateLastActivity();
}

void DCCTransfer::updateLastActivity() {
    _lastActivity = time(NULL);
}

bool DCCTransfer::isTimeout() const {
    if (_status == DCC_COMPLETED || _status == DCC_FAILED || _status == DCC_REJECTED) {
        return false;
    }
    return (time(NULL) - _lastActivity) > 300; // 5分のタイムアウト
}

bool DCCTransfer::isCompleted() const {
    return _status == DCC_COMPLETED;
}

void DCCTransfer::cleanup() {
    if (_listenSocket >= 0) {
        close(_listenSocket);
        _listenSocket = -1;
    }
    if (_dataSocket >= 0) {
        close(_dataSocket);
        _dataSocket = -1;
    }
    closeSendFile();
    closeReceiveFile();
}

// ゲッター実装
std::string DCCTransfer::getId() const { return _id; }
Client* DCCTransfer::getSender() const { return _sender; }
Client* DCCTransfer::getReceiver() const { return _receiver; }
std::string DCCTransfer::getFilename() const { return _filename; }
unsigned long DCCTransfer::getFilesize() const { return _filesize; }
unsigned long DCCTransfer::getBytesTransferred() const { return _bytesTransferred; }
DCCTransferType DCCTransfer::getType() const { return _type; }
DCCTransferStatus DCCTransfer::getStatus() const { return _status; }
int DCCTransfer::getListenSocket() const { return _listenSocket; }
int DCCTransfer::getDataSocket() const { return _dataSocket; }
int DCCTransfer::getPort() const { return _port; }
std::string DCCTransfer::getSenderIP() const { return _senderIP; }

double DCCTransfer::getProgress() const {
    if (_filesize == 0) return 100.0;
    return (double)_bytesTransferred / _filesize * 100.0;
}

double DCCTransfer::getTransferRate() const {
    time_t elapsed = time(NULL) - _startTime;
    if (elapsed == 0) return 0.0;
    return (double)_bytesTransferred / elapsed;
}

void DCCTransfer::setDataSocket(int socket) {
    _dataSocket = socket;
}

void DCCTransfer::setSenderIP(const std::string& ip) {
    _senderIP = ip;
}

std::string DCCTransfer::getStatusString() const {
    switch (_status) {
        case DCC_PENDING: return "PENDING";
        case DCC_ACTIVE: return "ACTIVE";
        case DCC_COMPLETED: return "COMPLETED";
        case DCC_FAILED: return "FAILED";
        case DCC_REJECTED: return "REJECTED";
        default: return "UNKNOWN";
    }
}

std::string DCCTransfer::getTransferInfo() const {
    std::stringstream ss;
    ss << "Transfer " << _id << ": " << _filename 
       << " (" << _bytesTransferred << "/" << _filesize << " bytes) "
       << "[" << getProgress() << "%] "
       << "Status: " << getStatusString();
    return ss.str();
}

// プライベートメソッドの実装

int DCCTransfer::createListenSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    // ソケットオプションの設定
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(sock);
        return -1;
    }
    
    // ポートの自動選択（範囲: 5000-5100）
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    
    for (int port = 5000; port <= 5100; port++) {
        addr.sin_port = htons(port);
        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            _port = port;
            break;
        }
    }
    
    if (_port == 0) {
        close(sock);
        return -1;
    }
    
    if (listen(sock, 1) < 0) {
        close(sock);
        return -1;
    }
    
    setSocketNonBlocking(sock);
    
    return sock;
}

bool DCCTransfer::setSocketNonBlocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) >= 0;
}

std::string DCCTransfer::getLocalIP() const {
    // 簡略化のため、ローカルホストを使用
    // 実際の実装では、適切なインターフェースのIPを取得する必要がある
    return "127.0.0.1";
}

bool DCCTransfer::openSendFile() {
    _sendFile = new std::ifstream(_filepath.c_str(), std::ios::binary);
    if (!_sendFile->is_open()) {
        delete _sendFile;
        _sendFile = NULL;
        return false;
    }
    return true;
}

bool DCCTransfer::openReceiveFile() {
    // 転送ディレクトリの作成
    system("mkdir -p ./dcc_transfers/received/");
    
    _recvFile = new std::ofstream(_filepath.c_str(), std::ios::binary);
    if (!_recvFile->is_open()) {
        delete _recvFile;
        _recvFile = NULL;
        return false;
    }
    return true;
}

void DCCTransfer::closeSendFile() {
    if (_sendFile) {
        _sendFile->close();
        delete _sendFile;
        _sendFile = NULL;
    }
}

void DCCTransfer::closeReceiveFile() {
    if (_recvFile) {
        // ファイルクローズ前に確実にバッファをフラッシュ
        _recvFile->flush();
        _recvFile->close();
        delete _recvFile;
        _recvFile = NULL;
    }
}

std::string DCCTransfer::generateTransferId() const {
    std::stringstream ss;
    ss << time(NULL) << "_" << rand() % 10000;
    return ss.str();
}

bool DCCTransfer::validateFilepath(const std::string& path) const {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        std::cout << "[DCC] File not found: " << path << " (errno: " << strerror(errno) << ")" << std::endl;
        return false;
    }
    
    // ファイルが通常ファイルであることを確認
    if (!S_ISREG(st.st_mode)) {
        std::cout << "[DCC] Not a regular file: " << path << std::endl;
        return false;
    }
    
    // ファイルサイズの確認
    if ((unsigned long)st.st_size != _filesize) {
        std::cout << "[DCC] File size mismatch: expected " << _filesize << ", got " << st.st_size << std::endl;
        return false;
    }
    
    std::cout << "[DCC] File validated successfully: " << path << " (size: " << st.st_size << ")" << std::endl;
    return true;
}
