#ifndef DCCMANAGER_HPP
# define DCCMANAGER_HPP

# include "Utils.hpp"
# include "DCCTransfer.hpp"
# include <map>
# include <vector>

class Server;
class Client;

class DCCManager {
private:
    Server*                                     _server;
    std::map<std::string, DCCTransfer*>        _transfers;         // 転送ID -> DCCTransfer
    std::map<int, DCCTransfer*>                _socketTransfers;   // ソケットFD -> DCCTransfer
    std::map<std::string, std::vector<std::string> > _pendingTransfers; // ニックネーム -> 転送ID
    int                                         _nextPort;          // 次に使用するポート
    static const int                            MIN_DCC_PORT = 5000;
    static const int                            MAX_DCC_PORT = 5100;
    static const time_t                         TRANSFER_TIMEOUT = 300; // 5分のタイムアウト

public:
    DCCManager(Server* server);
    ~DCCManager();

    // 転送の作成と管理
    std::string     createSendTransfer(Client* sender, Client* receiver, 
                                       const std::string& filename, unsigned long filesize);
    bool            acceptTransfer(Client* client, const std::string& transferId);
    bool            rejectTransfer(Client* client, const std::string& transferId);
    void            cancelTransfer(const std::string& transferId);
    
    // 転送の処理
    void            processTransfers();
    void            handleTransferSocket(int socket);
    void            checkTimeouts();
    
    // 転送情報の取得
    DCCTransfer*    getTransfer(const std::string& transferId);
    DCCTransfer*    getTransferBySocket(int socket);
    std::vector<DCCTransfer*> getClientTransfers(Client* client);
    std::vector<DCCTransfer*> getActiveTransfers();
    std::vector<DCCTransfer*> getPendingTransfers();
    
    // ソケット管理
    void            addTransferSocket(int socket, DCCTransfer* transfer);
    void            removeTransferSocket(int socket);
    std::vector<int> getTransferSockets();
    
    // クライアント管理
    void            removeClientTransfers(Client* client);
    bool            hasActiveTransfer(Client* client);
    
    // 統計情報
    size_t          getActiveTransferCount() const;
    size_t          getPendingTransferCount() const;
    size_t          getCompletedTransferCount() const;
    unsigned long   getTotalBytesTransferred() const;
    
    // 通知
    void            notifySendRequest(DCCTransfer* transfer);
    void            notifyTransferAccepted(DCCTransfer* transfer);
    void            notifyTransferRejected(DCCTransfer* transfer);
    void            notifyTransferComplete(DCCTransfer* transfer);
    void            notifyTransferFailed(DCCTransfer* transfer);
    void            notifyTransferProgress(DCCTransfer* transfer);
    
private:
    // ポート管理
    int             getAvailablePort();
    bool            isPortInUse(int port);
    
    // 転送管理
    void            addTransfer(DCCTransfer* transfer);
    void            removeTransfer(const std::string& transferId);
    void            cleanupTransfer(DCCTransfer* transfer);
    
    // ヘルパー関数
    std::string     formatFileSize(unsigned long size) const;
    std::string     formatTransferRate(double rate) const;
    bool            validateFile(const std::string& filepath, unsigned long maxSize) const;
};

#endif
