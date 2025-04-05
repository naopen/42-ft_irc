#ifndef CLIENT_HPP
# define CLIENT_HPP

# include "Utils.hpp"

class Channel;

enum ClientStatus {
    CONNECTING,  // 初期接続状態
    REGISTERING, // 登録中（PASS/NICK/USER処理中）
    REGISTERED   // 登録完了
};

class Client {
private:
    int             _fd;            // クライアントのソケットファイルディスクリプタ
    std::string     _nickname;      // ニックネーム
    std::string     _username;      // ユーザー名
    std::string     _hostname;      // ホスト名
    std::string     _realname;      // 本名
    std::string     _buffer;        // 受信バッファ
    ClientStatus    _status;        // クライアント状態
    bool            _passAccepted;  // パスワード認証済みフラグ
    std::vector<std::string> _channels; // 参加中のチャンネル
    bool            _operator;      // サーバーオペレータフラグ
    time_t          _lastActivity;  // 最終アクティビティ時間
    std::string     _awayMessage;   // 離席メッセージ
    bool            _away;          // 離席フラグ

public:
    Client(int fd, const std::string& hostname);
    ~Client();

    // ゲッター
    int             getFd() const;
    std::string     getNickname() const;
    std::string     getUsername() const;
    std::string     getHostname() const;
    std::string     getRealname() const;
    ClientStatus    getStatus() const;
    bool            isPassAccepted() const;
    bool            isOperator() const;
    time_t          getLastActivity() const;
    bool            isAway() const;
    std::string     getAwayMessage() const;
    std::vector<std::string> getChannels() const;
    std::string     getPrefix() const; // nickname!username@hostname 形式

    // セッター
    void            setNickname(const std::string& nickname);
    void            setUsername(const std::string& username);
    void            setRealname(const std::string& realname);
    void            setStatus(ClientStatus status);
    void            setPassAccepted(bool accepted);
    void            setOperator(bool op);
    void            updateLastActivity();
    void            setAway(bool away, const std::string& message = "");

    // チャンネル管理
    void            addChannel(const std::string& channel);
    void            removeChannel(const std::string& channel);
    bool            isInChannel(const std::string& channel) const;

    // バッファ操作
    void            appendToBuffer(const std::string& data);
    std::string     getBuffer() const;
    void            clearBuffer();
    std::vector<std::string> getCompleteMessages();

    // メッセージ送信
    void            sendMessage(const std::string& message);
    void            sendNumericReply(int code, const std::string& message);

    // ユーザー認証のための関数
    bool            isRegistered() const;
    bool            hasCompletedRegistration() const;
};

#endif
