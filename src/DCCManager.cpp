#include "../include/DCCManager.hpp"
#include "../include/Server.hpp"
#include "../include/Client.hpp"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <sys/stat.h>

DCCManager::DCCManager(Server* server) 
    : _server(server), _nextPort(MIN_DCC_PORT) {
}

DCCManager::~DCCManager() {
    // すべての転送をクリーンアップ
    for (std::map<std::string, DCCTransfer*>::iterator it = _transfers.begin(); 
         it != _transfers.end(); ++it) {
        delete it->second;
    }
    _transfers.clear();
    _socketTransfers.clear();
    _pendingTransfers.clear();
}

std::string DCCManager::createSendTransfer(Client* sender, Client* receiver, 
                                           const std::string& filename, unsigned long filesize) {
    (void)_server; // 将来の拡張用（サーバー設定やログ等）
    
    // ファイルサイズの制限チェック（100MBまで）
    const unsigned long MAX_FILE_SIZE = 100 * 1024 * 1024;
    if (filesize > MAX_FILE_SIZE) {
        return "";
    }
    
    // 同時転送数の制限（1クライアントあたり3つまで）
    if (getClientTransfers(sender).size() >= 3) {
        return "";
    }
    
    // 新しい転送を作成
    DCCTransfer* transfer = new DCCTransfer(sender, receiver, filename, filesize, DCC_SEND);
    
    // 送信の初期化
    if (!transfer->initializeSend()) {
        delete transfer;
        return "";
    }
    
    // 転送を追加
    addTransfer(transfer);
    
    // 受信者に通知
    notifySendRequest(transfer);
    
    return transfer->getId();
}

bool DCCManager::acceptTransfer(Client* client, const std::string& transferId) {
    DCCTransfer* transfer = getTransfer(transferId);
    if (!transfer) {
        return false;
    }
    
    // 権限チェック
    if (transfer->getReceiver() != client) {
        return false;
    }
    
    // ステータスチェック
    if (transfer->getStatus() != DCC_PENDING) {
        return false;
    }
    
    // 転送を受け入れる
    transfer->setStatus(DCC_ACTIVE);
    
    // 送信者に通知
    notifyTransferAccepted(transfer);
    
    return true;
}

bool DCCManager::rejectTransfer(Client* client, const std::string& transferId) {
    DCCTransfer* transfer = getTransfer(transferId);
    if (!transfer) {
        return false;
    }
    
    // 権限チェック
    if (transfer->getReceiver() != client) {
        return false;
    }
    
    // ステータスチェック
    if (transfer->getStatus() != DCC_PENDING) {
        return false;
    }
    
    // 転送を拒否
    transfer->setStatus(DCC_REJECTED);
    
    // 送信者に通知
    notifyTransferRejected(transfer);
    
    // 転送をクリーンアップ
    cleanupTransfer(transfer);
    
    return true;
}

void DCCManager::cancelTransfer(const std::string& transferId) {
    DCCTransfer* transfer = getTransfer(transferId);
    if (!transfer) {
        return;
    }
    
    transfer->setStatus(DCC_FAILED);
    notifyTransferFailed(transfer);
    cleanupTransfer(transfer);
}

void DCCManager::processTransfers() {
    // アクティブな転送を処理
    std::vector<DCCTransfer*> activeTransfers = getActiveTransfers();
    
    for (size_t i = 0; i < activeTransfers.size(); ++i) {
        DCCTransfer* transfer = activeTransfers[i];
        
        // 転送を処理
        if (transfer->processTransfer()) {
            // 進捗を通知（10%ごと）
            static std::map<std::string, int> lastProgress;
            int currentProgress = (int)(transfer->getProgress() / 10) * 10;
            
            if (lastProgress[transfer->getId()] != currentProgress) {
                lastProgress[transfer->getId()] = currentProgress;
                notifyTransferProgress(transfer);
            }
            
            // 完了チェック
            if (transfer->isCompleted()) {
                notifyTransferComplete(transfer);
                cleanupTransfer(transfer);
            }
        } else if (transfer->getStatus() == DCC_FAILED) {
            notifyTransferFailed(transfer);
            cleanupTransfer(transfer);
        }
    }
    
    // タイムアウトチェック
    checkTimeouts();
}

void DCCManager::handleTransferSocket(int socket) {
    DCCTransfer* transfer = getTransferBySocket(socket);
    if (!transfer) {
        return;
    }
    
    transfer->processTransfer();
    
    if (transfer->isCompleted()) {
        notifyTransferComplete(transfer);
        cleanupTransfer(transfer);
    } else if (transfer->getStatus() == DCC_FAILED) {
        notifyTransferFailed(transfer);
        cleanupTransfer(transfer);
    }
}

void DCCManager::checkTimeouts() {
    std::vector<std::string> timedOutTransfers;
    
    for (std::map<std::string, DCCTransfer*>::iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->isTimeout()) {
            timedOutTransfers.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < timedOutTransfers.size(); ++i) {
        DCCTransfer* transfer = getTransfer(timedOutTransfers[i]);
        if (transfer) {
            transfer->setStatus(DCC_FAILED);
            notifyTransferFailed(transfer);
            cleanupTransfer(transfer);
        }
    }
}

DCCTransfer* DCCManager::getTransfer(const std::string& transferId) {
    std::map<std::string, DCCTransfer*>::iterator it = _transfers.find(transferId);
    if (it != _transfers.end()) {
        return it->second;
    }
    return NULL;
}

DCCTransfer* DCCManager::getTransferBySocket(int socket) {
    std::map<int, DCCTransfer*>::iterator it = _socketTransfers.find(socket);
    if (it != _socketTransfers.end()) {
        return it->second;
    }
    return NULL;
}

std::vector<DCCTransfer*> DCCManager::getClientTransfers(Client* client) {
    std::vector<DCCTransfer*> clientTransfers;
    
    for (std::map<std::string, DCCTransfer*>::iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        DCCTransfer* transfer = it->second;
        if (transfer->getSender() == client || transfer->getReceiver() == client) {
            clientTransfers.push_back(transfer);
        }
    }
    
    return clientTransfers;
}

std::vector<DCCTransfer*> DCCManager::getActiveTransfers() {
    std::vector<DCCTransfer*> activeTransfers;
    
    for (std::map<std::string, DCCTransfer*>::iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->getStatus() == DCC_ACTIVE || 
            it->second->getStatus() == DCC_PENDING) {
            activeTransfers.push_back(it->second);
        }
    }
    
    return activeTransfers;
}

std::vector<DCCTransfer*> DCCManager::getPendingTransfers() {
    std::vector<DCCTransfer*> pendingTransfers;
    
    for (std::map<std::string, DCCTransfer*>::iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->getStatus() == DCC_PENDING) {
            pendingTransfers.push_back(it->second);
        }
    }
    
    return pendingTransfers;
}

void DCCManager::addTransferSocket(int socket, DCCTransfer* transfer) {
    _socketTransfers[socket] = transfer;
}

void DCCManager::removeTransferSocket(int socket) {
    _socketTransfers.erase(socket);
}

std::vector<int> DCCManager::getTransferSockets() {
    std::vector<int> sockets;
    
    for (std::map<int, DCCTransfer*>::iterator it = _socketTransfers.begin();
         it != _socketTransfers.end(); ++it) {
        sockets.push_back(it->first);
    }
    
    return sockets;
}

void DCCManager::removeClientTransfers(Client* client) {
    std::vector<DCCTransfer*> clientTransfers = getClientTransfers(client);
    
    for (size_t i = 0; i < clientTransfers.size(); ++i) {
        DCCTransfer* transfer = clientTransfers[i];
        transfer->setStatus(DCC_FAILED);
        cleanupTransfer(transfer);
    }
}

bool DCCManager::hasActiveTransfer(Client* client) {
    std::vector<DCCTransfer*> transfers = getClientTransfers(client);
    
    for (size_t i = 0; i < transfers.size(); ++i) {
        if (transfers[i]->getStatus() == DCC_ACTIVE) {
            return true;
        }
    }
    
    return false;
}

size_t DCCManager::getActiveTransferCount() const {
    size_t count = 0;
    for (std::map<std::string, DCCTransfer*>::const_iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->getStatus() == DCC_ACTIVE) {
            count++;
        }
    }
    return count;
}

size_t DCCManager::getPendingTransferCount() const {
    size_t count = 0;
    for (std::map<std::string, DCCTransfer*>::const_iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->getStatus() == DCC_PENDING) {
            count++;
        }
    }
    return count;
}

size_t DCCManager::getCompletedTransferCount() const {
    size_t count = 0;
    for (std::map<std::string, DCCTransfer*>::const_iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->getStatus() == DCC_COMPLETED) {
            count++;
        }
    }
    return count;
}

unsigned long DCCManager::getTotalBytesTransferred() const {
    unsigned long total = 0;
    for (std::map<std::string, DCCTransfer*>::const_iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        total += it->second->getBytesTransferred();
    }
    return total;
}

void DCCManager::notifySendRequest(DCCTransfer* transfer) {
    if (!transfer) return;
    
    Client* receiver = transfer->getReceiver();
    Client* sender = transfer->getSender();
    
    if (!receiver || !sender) return;
    
    // DCC SEND通知をCTCPメッセージとして送信
    std::stringstream ss;
    ss << ":" << sender->getPrefix() << " PRIVMSG " << receiver->getNickname()
       << " :\001DCC SEND " << transfer->getFilename() 
       << " " << inet_addr(transfer->getSenderIP().c_str())
       << " " << transfer->getPort()
       << " " << transfer->getFilesize()
       << " " << transfer->getId() << "\001\r\n";
    
    receiver->sendMessage(ss.str());
}

void DCCManager::notifyTransferAccepted(DCCTransfer* transfer) {
    if (!transfer) return;
    
    Client* sender = transfer->getSender();
    if (!sender) return;
    
    std::stringstream ss;
    ss << ":server NOTICE " << sender->getNickname()
       << " :DCC SEND accepted by " << transfer->getReceiver()->getNickname()
       << " for file " << transfer->getFilename() << "\r\n";
    
    sender->sendMessage(ss.str());
}

void DCCManager::notifyTransferRejected(DCCTransfer* transfer) {
    if (!transfer) return;
    
    Client* sender = transfer->getSender();
    if (!sender) return;
    
    std::stringstream ss;
    ss << ":server NOTICE " << sender->getNickname()
       << " :DCC SEND rejected by " << transfer->getReceiver()->getNickname()
       << " for file " << transfer->getFilename() << "\r\n";
    
    sender->sendMessage(ss.str());
}

void DCCManager::notifyTransferComplete(DCCTransfer* transfer) {
    if (!transfer) return;
    
    Client* sender = transfer->getSender();
    Client* receiver = transfer->getReceiver();
    
    std::stringstream ss;
    ss << ":server NOTICE " << sender->getNickname()
       << " :DCC SEND completed: " << transfer->getFilename()
       << " (" << formatFileSize(transfer->getFilesize()) << ") "
       << "to " << receiver->getNickname() << "\r\n";
    
    sender->sendMessage(ss.str());
    
    ss.str("");
    ss << ":server NOTICE " << receiver->getNickname()
       << " :DCC GET completed: " << transfer->getFilename()
       << " (" << formatFileSize(transfer->getFilesize()) << ") "
       << "from " << sender->getNickname() << "\r\n";
    
    receiver->sendMessage(ss.str());
}

void DCCManager::notifyTransferFailed(DCCTransfer* transfer) {
    if (!transfer) return;
    
    Client* sender = transfer->getSender();
    Client* receiver = transfer->getReceiver();
    
    std::string reason = transfer->isTimeout() ? "timeout" : "error";
    
    if (sender) {
        std::stringstream ss;
        ss << ":server NOTICE " << sender->getNickname()
           << " :DCC SEND failed (" << reason << "): " << transfer->getFilename() << "\r\n";
        sender->sendMessage(ss.str());
    }
    
    if (receiver) {
        std::stringstream ss;
        ss << ":server NOTICE " << receiver->getNickname()
           << " :DCC GET failed (" << reason << "): " << transfer->getFilename() << "\r\n";
        receiver->sendMessage(ss.str());
    }
}

void DCCManager::notifyTransferProgress(DCCTransfer* transfer) {
    if (!transfer || transfer->getStatus() != DCC_ACTIVE) return;
    
    // 送信者と受信者の両方に進捗を通知
    Client* sender = transfer->getSender();
    Client* receiver = transfer->getReceiver();
    
    std::stringstream ss;
    ss << ":server NOTICE " << sender->getNickname()
       << " :DCC Transfer progress: " << transfer->getFilename()
       << " [" << std::fixed << std::setprecision(1) << transfer->getProgress() << "%] "
       << "(" << formatFileSize(transfer->getBytesTransferred()) 
       << "/" << formatFileSize(transfer->getFilesize()) << ") "
       << "Speed: " << formatTransferRate(transfer->getTransferRate()) << "\r\n";
    
    if (sender) sender->sendMessage(ss.str());
    
    ss.str("");
    ss << ":server NOTICE " << receiver->getNickname()
       << " :DCC Transfer progress: " << transfer->getFilename()
       << " [" << std::fixed << std::setprecision(1) << transfer->getProgress() << "%] "
       << "(" << formatFileSize(transfer->getBytesTransferred()) 
       << "/" << formatFileSize(transfer->getFilesize()) << ") "
       << "Speed: " << formatTransferRate(transfer->getTransferRate()) << "\r\n";
    
    if (receiver) receiver->sendMessage(ss.str());
}

// プライベートメソッドの実装

int DCCManager::getAvailablePort() {
    for (int port = _nextPort; port <= MAX_DCC_PORT; port++) {
        if (!isPortInUse(port)) {
            _nextPort = port + 1;
            if (_nextPort > MAX_DCC_PORT) {
                _nextPort = MIN_DCC_PORT;
            }
            return port;
        }
    }
    
    // 最初から再検索
    for (int port = MIN_DCC_PORT; port < _nextPort; port++) {
        if (!isPortInUse(port)) {
            _nextPort = port + 1;
            return port;
        }
    }
    return -1;
}

bool DCCManager::isPortInUse(int port) {
    for (std::map<std::string, DCCTransfer*>::iterator it = _transfers.begin();
         it != _transfers.end(); ++it) {
        if (it->second->getPort() == port && 
            (it->second->getStatus() == DCC_PENDING || 
             it->second->getStatus() == DCC_ACTIVE)) {
            return true;
        }
    }
    return false;
}

void DCCManager::addTransfer(DCCTransfer* transfer) {
    if (!transfer) return;
    
    _transfers[transfer->getId()] = transfer;
    
    // ソケットマッピングを追加
    if (transfer->getListenSocket() >= 0) {
        addTransferSocket(transfer->getListenSocket(), transfer);
    }
    if (transfer->getDataSocket() >= 0) {
        addTransferSocket(transfer->getDataSocket(), transfer);
    }
    
    // ペンディング転送リストに追加
    _pendingTransfers[transfer->getReceiver()->getNickname()].push_back(transfer->getId());
}

void DCCManager::removeTransfer(const std::string& transferId) {
    std::map<std::string, DCCTransfer*>::iterator it = _transfers.find(transferId);
    if (it == _transfers.end()) return;
    
    DCCTransfer* transfer = it->second;
    
    // ソケットマッピングを削除
    if (transfer->getListenSocket() >= 0) {
        removeTransferSocket(transfer->getListenSocket());
    }
    if (transfer->getDataSocket() >= 0) {
        removeTransferSocket(transfer->getDataSocket());
    }
    
    // ペンディング転送リストから削除
    if (transfer->getReceiver()) {
        std::vector<std::string>& pending = _pendingTransfers[transfer->getReceiver()->getNickname()];
        pending.erase(std::remove(pending.begin(), pending.end(), transferId), pending.end());
    }
    
    // 転送を削除
    delete transfer;
    _transfers.erase(it);
}

void DCCManager::cleanupTransfer(DCCTransfer* transfer) {
    if (!transfer) return;
    
    transfer->cleanup();
    removeTransfer(transfer->getId());
}

std::string DCCManager::formatFileSize(unsigned long size) const {
    std::stringstream ss;
    
    if (size >= 1024 * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) 
           << (double)size / (1024 * 1024 * 1024) << " GB";
    } else if (size >= 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) 
           << (double)size / (1024 * 1024) << " MB";
    } else if (size >= 1024) {
        ss << std::fixed << std::setprecision(2) 
           << (double)size / 1024 << " KB";
    } else {
        ss << size << " B";
    }
    
    return ss.str();
}

std::string DCCManager::formatTransferRate(double rate) const {
    std::stringstream ss;
    
    if (rate >= 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) 
           << rate / (1024 * 1024) << " MB/s";
    } else if (rate >= 1024) {
        ss << std::fixed << std::setprecision(2) 
           << rate / 1024 << " KB/s";
    } else {
        ss << std::fixed << std::setprecision(0) 
           << rate << " B/s";
    }
    
    return ss.str();
}

bool DCCManager::validateFile(const std::string& filepath, unsigned long maxSize) const {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        return false;
    }
    
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    if ((unsigned long)st.st_size > maxSize) {
        return false;
    }
    
    return true;
}
