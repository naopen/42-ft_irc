#ifndef DCCTRANSFER_HPP
# define DCCTRANSFER_HPP

# include "Utils.hpp"
# include <fstream>
# include <sys/stat.h>
# include <arpa/inet.h>
# include <fcntl.h>

class Client;

enum DCCTransferType {
    DCC_SEND,
    DCC_GET
};

enum DCCTransferStatus {
    DCC_PENDING,    // 転送待機中
    DCC_ACTIVE,     // 転送中
    DCC_COMPLETED,  // 完了
    DCC_FAILED,     // 失敗
    DCC_REJECTED    // 拒否
};

class DCCTransfer {
private:
    std::string         _id;            // 転送ID（ユニーク識別子）
    Client*             _sender;        // 送信者
    Client*             _receiver;      // 受信者
    std::string         _filename;      // ファイル名
    std::string         _filepath;      // フルパス
    unsigned long       _filesize;      // ファイルサイズ
    unsigned long       _bytesTransferred; // 転送済みバイト数
    DCCTransferType     _type;          // 転送タイプ（SEND/GET）
    DCCTransferStatus   _status;        // 転送状態
    int                 _listenSocket;   // リスニングソケット（送信側）
    int                 _dataSocket;     // データ転送ソケット
    int                 _port;           // DCC用ポート
    std::string         _senderIP;      // 送信者IP
    time_t              _startTime;     // 転送開始時刻
    time_t              _lastActivity;  // 最終活動時刻
    std::ifstream*      _sendFile;      // 送信用ファイルストリーム
    std::ofstream*      _recvFile;      // 受信用ファイルストリーム
    char*               _buffer;        // 転送バッファ
    static const size_t DCC_BUFFER_SIZE = 8192; // バッファサイズ
    static const size_t DCC_FLUSH_INTERVAL = 65536; // フラッシュ間隔（64KB）
    unsigned long       _lastFlushBytes; // 最後にフラッシュした時点のバイト数

public:
    DCCTransfer(Client* sender, Client* receiver, const std::string& filename, 
                unsigned long filesize, DCCTransferType type);
    ~DCCTransfer();

    // 転送の初期化と開始
    bool            initializeSend();
    bool            initializeReceive(const std::string& ip, int port);
    bool            acceptConnection();
    
    // データ転送
    bool            sendData();
    bool            receiveData();
    bool            processTransfer();
    
    // 状態管理
    void            setStatus(DCCTransferStatus status);
    void            updateLastActivity();
    bool            isTimeout() const;
    bool            isCompleted() const;
    void            cleanup();
    
    // ゲッター
    std::string     getId() const;
    Client*         getSender() const;
    Client*         getReceiver() const;
    std::string     getFilename() const;
    unsigned long   getFilesize() const;
    unsigned long   getBytesTransferred() const;
    DCCTransferType getType() const;
    DCCTransferStatus getStatus() const;
    int             getListenSocket() const;
    int             getDataSocket() const;
    int             getPort() const;
    std::string     getSenderIP() const;
    double          getProgress() const;
    double          getTransferRate() const;
    
    // セッター
    void            setDataSocket(int socket);
    void            setSenderIP(const std::string& ip);
    
    // ヘルパー関数
    std::string     getStatusString() const;
    std::string     getTransferInfo() const;
    
private:
    // ソケット操作
    int             createListenSocket();
    bool            setSocketNonBlocking(int socket);
    std::string     getLocalIP() const;
    
    // ファイル操作
    bool            openSendFile();
    bool            openReceiveFile();
    void            closeSendFile();
    void            closeReceiveFile();
    
    // ユーティリティ
    std::string     generateTransferId() const;
    bool            validateFilepath(const std::string& path) const;
};

#endif
