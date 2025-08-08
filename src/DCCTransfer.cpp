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
      _dataSocket(-1), _port(0), _sendFile(NULL), _recvFile(NULL), _buffer(NULL) {
    
    _id = generateTransferId();
    _startTime = time(NULL);
    _lastActivity = _startTime;
    _buffer = new char[DCC_BUFFER_SIZE];
    
    // ファイルパスの設定（セキュリティのため、転送ディレクトリに制限）
    _filepath = "./dcc_transfers/" + _filename;
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
        return false;
    }
    
    // ファイルの存在確認
    if (!validateFilepath(_filepath)) {
        _status = DCC_FAILED;
        return false;
    }
    
    // 送信用ファイルを開く
    if (!openSendFile()) {
        _status = DCC_FAILED;
        return false;
    }
    
    // リスニングソケットの作成
    _listenSocket = createListenSocket();
    if (_listenSocket < 0) {
        closeSendFile();
        _status = DCC_FAILED;
        return false;
    }
    
    // 送信者IPの取得
    _senderIP = getLocalIP();
    
    return true;
}

bool DCCTransfer::initializeReceive(const std::string& ip, int port) {
    if (_type != DCC_GET) {
        return false;
    }
    
    _senderIP = ip;
    _port = port;
    
    // 受信用ファイルを開く
    if (!openReceiveFile()) {
        _status = DCC_FAILED;
        return false;
    }
    
    // 送信者に接続
    _dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_dataSocket < 0) {
        closeReceiveFile();
        _status = DCC_FAILED;
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = inet_addr(_senderIP.c_str());
    
    if (connect(_dataSocket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(_dataSocket);
        _dataSocket = -1;
        closeReceiveFile();
        _status = DCC_FAILED;
        return false;
    }
    
    setSocketNonBlocking(_dataSocket);
    _status = DCC_ACTIVE;
    
    return true;
}

bool DCCTransfer::acceptConnection() {
    if (_listenSocket < 0 || _type != DCC_SEND) {
        return false;
    }
    
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    _dataSocket = accept(_listenSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (_dataSocket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            _status = DCC_FAILED;
            return false;
        }
        return false; // まだ接続がない
    }
    
    setSocketNonBlocking(_dataSocket);
    
    // リスニングソケットを閉じる
    close(_listenSocket);
    _listenSocket = -1;
    
    _status = DCC_ACTIVE;
    updateLastActivity();
    
    return true;
}

bool DCCTransfer::sendData() {
    if (_status != DCC_ACTIVE || !_sendFile || _dataSocket < 0) {
        return false;
    }
    
    if (_sendFile->eof() || _bytesTransferred >= _filesize) {
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
                _status = DCC_COMPLETED;
            }
            return true;
        } else if (bytesSent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
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
        
        // 受信確認の送信（DCC プロトコル）
        uint32_t ack = htonl(_bytesTransferred);
        send(_dataSocket, &ack, sizeof(ack), MSG_NOSIGNAL);
        
        if (_bytesTransferred >= _filesize) {
            _status = DCC_COMPLETED;
        }
        return true;
    } else if (bytesReceived == 0) {
        // 接続が閉じられた
        if (_bytesTransferred >= _filesize) {
            _status = DCC_COMPLETED;
        } else {
            _status = DCC_FAILED;
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
        return acceptConnection();
    } else if (_status == DCC_ACTIVE) {
        if (_type == DCC_SEND) {
            return sendData();
        } else {
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
    std::string receivePath = "./dcc_transfers/received/" + _filename;
    
    _recvFile = new std::ofstream(receivePath.c_str(), std::ios::binary);
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
        return false;
    }
    
    // ファイルが通常ファイルであることを確認
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    // ファイルサイズの確認
    if ((unsigned long)st.st_size != _filesize) {
        return false;
    }
    
    return true;
}
