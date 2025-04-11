#include "../../include/Command.hpp"
#include "../../include/Server.hpp"

// PASS コマンド
PassCommand::PassCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "PASS", params)
{
    _requiresRegistration = false;
}

PassCommand::~PassCommand() {
    // 特に何もしない
}

void PassCommand::execute() {
    // 既に認証済みの場合はエラー
    if (_client->isRegistered()) {
        _client->sendNumericReply(ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
        return;
    }

    std::string password = _params[0];

    // パスワードのチェック
    if (!_server->checkPassword(password)) {
        _client->sendNumericReply(ERR_PASSWDMISMATCH, ":Password incorrect");
        return;
    }

    // パスワード認証成功
    _client->setPassAccepted(true);

    // パスワード認証成功のメッセージを送信
    _client->sendMessage(":" + _server->getHostname() + " NOTICE Auth :Password accepted");

    // NICK と USER が既に設定されている場合は登録完了
    if (!_client->getNickname().empty() && !_client->getUsername().empty()) {
        _client->setStatus(REGISTERED);

        // ウェルカムメッセージを送信
        std::string welcomeMsg = ":Welcome to the Internet Relay Network " + _client->getPrefix();
        std::string hostMsg = ":Your host is " + _server->getHostname() + ", running version " + std::string(IRC_VERSION);
        std::string createdMsg = ":This server was created " + std::string(IRC_CREATION_DATE);
        std::string infoMsg = _server->getHostname() + " " + std::string(IRC_VERSION) + " o mtikl";

        _client->sendNumericReply(RPL_WELCOME, welcomeMsg);
        _client->sendNumericReply(RPL_YOURHOST, hostMsg);
        _client->sendNumericReply(RPL_CREATED, createdMsg);
        _client->sendNumericReply(RPL_MYINFO, infoMsg);
    }
}

// NICK コマンド
NickCommand::NickCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "NICK", params)
{
    _requiresRegistration = false;
}

NickCommand::~NickCommand() {
    // 特に何もしない
}

void NickCommand::execute() {
    // パラメータが不足している場合はエラー
    if (_params.empty()) {
        _client->sendNumericReply(ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }

    std::string nickname = _params[0];

    // デバッグ出力
    std::cout << "NICK command: Client wants to change nickname to " << nickname << std::endl;

    // ニックネームの長さをチェック
    if (nickname.length() > 9) {
        _client->sendNumericReply(ERR_ERRONEUSNICKNAME, nickname + " :Erroneous nickname");
        return;
    }

    // ニックネームの文字をチェック（英数字とハイフン、アンダースコアのみ許可）
    for (size_t i = 0; i < nickname.length(); i++) {
        char c = nickname[i];
        if (!isalnum(c) && c != '-' && c != '_') {
            _client->sendNumericReply(ERR_ERRONEUSNICKNAME, nickname + " :Erroneous nickname");
            return;
        }
    }

    // ニックネームが既に使用されている場合はエラー
    if (_server->isNicknameInUse(nickname) && nickname != _client->getNickname()) {
        _client->sendNumericReply(ERR_NICKNAMEINUSE, nickname + " :Nickname is already in use");
        std::cout << "Nickname " << nickname << " is already in use" << std::endl;
        return;
    }

    // 古いニックネームを保存
    std::string oldNick = _client->getNickname();

    std::cout << "Updating nickname from '" << oldNick << "' to '" << nickname << "'" << std::endl;

    // サーバーのニックネームマップを更新
    if (!oldNick.empty()) {
        _server->updateNickname(oldNick, nickname);
    } else {
        // 新しいニックネームをマップに追加
        _server->_nicknames[nickname] = _client;
    }

    // ニックネームを更新
    _client->setNickname(nickname);

    // 古いニックネームがある場合は変更通知を送信
    if (!oldNick.empty()) {
        std::string message = ":" + oldNick + "!" + _client->getUsername() + "@" + _client->getHostname() + " NICK :" + nickname;
        _client->sendMessage(message);
    }

    // PASS と USER が既に設定されている場合は登録完了
    if (_client->isPassAccepted() && !_client->getUsername().empty() && _client->getStatus() != REGISTERED) {
        _client->setStatus(REGISTERED);

        // ウェルカムメッセージを送信
        std::string welcomeMsg = ":Welcome to the Internet Relay Network " + _client->getPrefix();
        std::string hostMsg = ":Your host is " + _server->getHostname() + ", running version " + std::string(IRC_VERSION);
        std::string createdMsg = ":This server was created " + std::string(IRC_CREATION_DATE);
        std::string infoMsg = _server->getHostname() + " " + std::string(IRC_VERSION) + " o mtikl";

        _client->sendNumericReply(RPL_WELCOME, welcomeMsg);
        _client->sendNumericReply(RPL_YOURHOST, hostMsg);
        _client->sendNumericReply(RPL_CREATED, createdMsg);
        _client->sendNumericReply(RPL_MYINFO, infoMsg);
    }
}

// USER コマンド
UserCommand::UserCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "USER", params)
{
    _requiresRegistration = false;
}

UserCommand::~UserCommand() {
    // 特に何もしない
}

void UserCommand::execute() {
    // 既に認証済みの場合はエラー
    if (!_client->getUsername().empty()) {
        _client->sendNumericReply(ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }

    // パラメータが不足している場合はエラー
    if (_params.size() < 4) {
        _client->sendNumericReply(ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
        return;
    }

    std::string username = _params[0];
    //std::string hostname = _params[1]; // ホスト名は無視
    //std::string servername = _params[2]; // サーバー名は無視
    std::string realname = _params[3];

    // 最初の文字が:の場合は削除
    if (!realname.empty() && realname[0] == ':') {
        realname = realname.substr(1);
    }

    // ユーザー名と本名を設定
    _client->setUsername(username);
    _client->setRealname(realname);

    // PASS と NICK が既に設定されている場合は登録完了
    if (_client->isPassAccepted() && !_client->getNickname().empty() && _client->getStatus() != REGISTERED) {
        _client->setStatus(REGISTERED);

        // ウェルカムメッセージを送信
        std::string welcomeMsg = ":Welcome to the Internet Relay Network " + _client->getPrefix();
        std::string hostMsg = ":Your host is " + _server->getHostname() + ", running version " + std::string(IRC_VERSION);
        std::string createdMsg = ":This server was created " + std::string(IRC_CREATION_DATE);
        std::string infoMsg = _server->getHostname() + " " + std::string(IRC_VERSION) + " o mtikl";

        _client->sendNumericReply(RPL_WELCOME, welcomeMsg);
        _client->sendNumericReply(RPL_YOURHOST, hostMsg);
        _client->sendNumericReply(RPL_CREATED, createdMsg);
        _client->sendNumericReply(RPL_MYINFO, infoMsg);
    }
}