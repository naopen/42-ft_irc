#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include "Utils.hpp"
# include "Client.hpp"

class Channel {
private:
    std::string _name;                          // チャンネル名
    std::string _topic;                         // チャンネルトピック
    std::string _key;                           // チャンネルパスワード
    std::vector<Client*> _clients;              // チャンネル参加者
    std::vector<std::string> _operators;        // チャンネルオペレータのニックネーム
    std::vector<std::string> _invitedUsers;     // 招待済みユーザーのニックネーム
    bool _inviteOnly;                           // 招待のみモード
    bool _topicRestricted;                      // トピック制限モード
    size_t _userLimit;                          // ユーザー数制限
    bool _hasUserLimit;                         // ユーザー制限有無フラグ
    time_t _creationTime;                       // チャンネル作成時間

public:
    Channel(const std::string& name, Client* creator);
    ~Channel();

    // ゲッター
    std::string     getName() const;
    std::string     getTopic() const;
    std::string     getKey() const;
    std::vector<Client*> getClients() const;
    bool            isInviteOnly() const;
    bool            isTopicRestricted() const;
    bool            hasKey() const;
    bool            hasUserLimit() const;
    size_t          getUserLimit() const;
    time_t          getCreationTime() const;

    // セッター
    void            setTopic(const std::string& topic);
    void            setKey(const std::string& key);
    void            setInviteOnly(bool inviteOnly);
    void            setTopicRestricted(bool restricted);
    void            setUserLimit(size_t limit);

    // ユーザー管理
    bool            addClient(Client* client, const std::string& key = "");
    void            removeClient(Client* client);
    bool            isClientInChannel(Client* client) const;
    bool            isClientInChannel(const std::string& nickname) const;

    // オペレータ管理
    bool            isOperator(const std::string& nickname) const;
    void            addOperator(const std::string& nickname);
    void            removeOperator(const std::string& nickname);

    // 招待管理
    void            inviteUser(const std::string& nickname);
    bool            isInvited(const std::string& nickname) const;
    void            removeInvite(const std::string& nickname);

    // メッセージ送信
    void            broadcastMessage(const std::string& message, Client* exclude = NULL);
    void            sendNames(Client* client);

    // モード管理
    std::string     getModes() const;
    bool            applyMode(char mode, bool set, const std::string& param = "", Client* client = NULL);

    // ユーティリティ
    size_t          getClientCount() const;
};

#endif
