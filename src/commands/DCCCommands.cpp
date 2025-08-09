#include "../include/DCCCommand.hpp"
#include "../include/Server.hpp"
#include "../include/Client.hpp"
#include <sstream>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>

// DCC SEND コマンドの実装
DCCSendCommand::DCCSendCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "DCC", params) {
    _requiresRegistration = true;
}

DCCSendCommand::~DCCSendCommand() {}

void DCCSendCommand::execute() {
    if (!canExecute()) {
        _client->sendNumericReply(451, ":You have not registered");
        return;
    }
    
    // パラメータチェック: DCC SEND <nickname> <filepath>
    // 注: Command.cppでサブコマンド(SEND)は既に除外されているので、_paramsには[nickname, filepath]のみ
    if (_params.size() < 2) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Usage: DCC SEND <nickname> <filepath>\r\n");
        return;
    }
    
    std::string targetNick = _params[0];
    std::string filepath = _params[1];
    
    // 受信者の確認
    Client* receiver = _server->getClientByNickname(targetNick);
    if (!receiver) {
        _client->sendNumericReply(401, targetNick + " :No such nick/channel");
        return;
    }
    
    // 自分自身には送れない
    if (receiver == _client) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Cannot send file to yourself\r\n");
        return;
    }
    
    // ファイルの存在とサイズを確認
    if (!validateFilepath(filepath)) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :File not found or inaccessible: " + filepath + "\r\n");
        return;
    }
    
    unsigned long filesize = getFileSize(filepath);
    if (filesize == 0) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Cannot send empty file\r\n");
        return;
    }
    
    // ファイルサイズ制限（100MB）
    const unsigned long MAX_SIZE = 100 * 1024 * 1024;
    if (filesize > MAX_SIZE) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :File too large (max 100MB)\r\n");
        return;
    }
    
    // ファイル名の抽出
    std::string filename = parseFilename(filepath);
    
    // DCCManager を取得または作成
    DCCManager* dccManager = _server->getDCCManager();
    if (!dccManager) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC not available on this server\r\n");
        return;
    }
    
    // 転送を作成
    std::string transferId = dccManager->createSendTransfer(_client, receiver, filename, filesize);
    if (transferId.empty()) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Failed to create DCC transfer\r\n");
        return;
    }
    
    _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                       " :DCC SEND request sent to " + targetNick + 
                       " for file " + filename + " (ID: " + transferId + ")\r\n");
}

std::string DCCSendCommand::parseFilename(const std::string& path) const {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

unsigned long DCCSendCommand::getFileSize(const std::string& filepath) const {
    struct stat st;
    if (stat(filepath.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

bool DCCSendCommand::validateFilepath(const std::string& filepath) const {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        return false;
    }
    
    // 通常ファイルであることを確認
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    
    // 読み取り権限があることを確認
    if (access(filepath.c_str(), R_OK) != 0) {
        return false;
    }
    
    return true;
}

std::string DCCSendCommand::convertIPToLong(const std::string& ip) const {
    unsigned long addr = inet_addr(ip.c_str());
    std::stringstream ss;
    ss << ntohl(addr);
    return ss.str();
}

// DCC GET/ACCEPT コマンドの実装
DCCGetCommand::DCCGetCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "DCC", params) {
    _requiresRegistration = true;
}

DCCGetCommand::~DCCGetCommand() {}

void DCCGetCommand::execute() {
    if (!canExecute()) {
        _client->sendNumericReply(451, ":You have not registered");
        return;
    }
    
    // パラメータチェック: DCC GET <transferId> または DCC GET <nickname> <filename>
    // 注: Command.cppでサブコマンド(GET/ACCEPT)は既に除外されているので、_paramsには[transferId]または[nickname, filename]のみ
    if (_params.size() < 1) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Usage: DCC GET <transferId> or DCC GET <nickname> <filename>\r\n");
        return;
    }
    
    DCCManager* dccManager = _server->getDCCManager();
    if (!dccManager) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC not available on this server\r\n");
        return;
    }
    
    std::string transferId;
    
    // 2つのパラメータがある場合: nickname と filename
    if (_params.size() >= 2) {
        std::string senderNick = _params[0];
        std::string filename = _params[1];
        
        // 送信者の確認
        Client* sender = _server->getClientByNickname(senderNick);
        if (!sender) {
            _client->sendNumericReply(401, senderNick + " :No such nick/channel");
            return;
        }
        
        // そのニックネームからの保留中の転送を検索
        transferId = dccManager->findPendingTransferBySenderAndFile(sender, _client, filename);
        if (transferId.empty()) {
            // 保留中の転送がない場合、プル型のリクエストを作成
            // これは受信者が送信者にファイルを要求するケース
            _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                               " :Creating DCC GET request to " + senderNick + 
                               " for file " + filename + "\r\n");
            
            // 送信者にプルリクエストを通知
            std::string reqMsg = ":" + _client->getPrefix() + " PRIVMSG " + senderNick + 
                                 " :\001DCC GET " + filename + "\001\r\n";
            sender->sendMessage(reqMsg);
            
            _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                               " :DCC GET request sent to " + senderNick + 
                               ". Waiting for response.\r\n");
            return;
        }
    }
    // 1つのパラメータのみの場合: transferId
    else {
        if (!parseTransferInfo(_params[0], transferId)) {
            _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                               " :Invalid transfer ID\r\n");
            return;
        }
    }
    
    if (dccManager->acceptTransfer(_client, transferId)) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC transfer accepted (ID: " + transferId + ")\r\n");
    } else {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Failed to accept DCC transfer\r\n");
    }
}

bool DCCGetCommand::parseTransferInfo(const std::string& info, std::string& transferId) {
    // 単純な転送IDの場合
    transferId = info;
    return !transferId.empty();
}

// DCC REJECT コマンドの実装
DCCRejectCommand::DCCRejectCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "DCC", params) {
    _requiresRegistration = true;
}

DCCRejectCommand::~DCCRejectCommand() {}

void DCCRejectCommand::execute() {
    if (!canExecute()) {
        _client->sendNumericReply(451, ":You have not registered");
        return;
    }
    
    // 注: Command.cppでサブコマンド(REJECT)は既に除外されているので、_paramsには[transferId]のみ
    if (_params.size() < 1) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Usage: DCC REJECT <transferId>\r\n");
        return;
    }
    
    std::string transferId = _params[0];
    
    DCCManager* dccManager = _server->getDCCManager();
    if (!dccManager) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC not available on this server\r\n");
        return;
    }
    
    if (dccManager->rejectTransfer(_client, transferId)) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC transfer rejected (ID: " + transferId + ")\r\n");
    } else {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Failed to reject DCC transfer\r\n");
    }
}

// DCC LIST コマンドの実装
DCCListCommand::DCCListCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "DCC", params) {
    _requiresRegistration = true;
}

DCCListCommand::~DCCListCommand() {}

void DCCListCommand::execute() {
    if (!canExecute()) {
        _client->sendNumericReply(451, ":You have not registered");
        return;
    }
    
    DCCManager* dccManager = _server->getDCCManager();
    if (!dccManager) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC not available on this server\r\n");
        return;
    }
    
    std::vector<DCCTransfer*> transfers = dccManager->getClientTransfers(_client);
    
    if (transfers.empty()) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :No active DCC transfers\r\n");
        return;
    }
    
    _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                       " :=== DCC Transfer List ===\r\n");
    
    for (size_t i = 0; i < transfers.size(); ++i) {
        DCCTransfer* transfer = transfers[i];
        std::stringstream ss;
        ss << ":server NOTICE " << _client->getNickname() << " :"
           << "[" << transfer->getId() << "] "
           << (transfer->getType() == DCC_SEND ? "SEND" : "GET") << " "
           << transfer->getFilename() << " "
           << "(" << transfer->getBytesTransferred() << "/" << transfer->getFilesize() << " bytes) "
           << "[" << transfer->getProgress() << "%] "
           << "Status: " << transfer->getStatusString() << "\r\n";
        _client->sendMessage(ss.str());
    }
    
    _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                       " :=========================\r\n");
}

// DCC CANCEL コマンドの実装
DCCCancelCommand::DCCCancelCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "DCC", params) {
    _requiresRegistration = true;
}

DCCCancelCommand::~DCCCancelCommand() {}

void DCCCancelCommand::execute() {
    if (!canExecute()) {
        _client->sendNumericReply(451, ":You have not registered");
        return;
    }
    
    // 注: Command.cppでサブコマンド(CANCEL)は既に除外されているので、_paramsには[transferId]のみ
    if (_params.size() < 1) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Usage: DCC CANCEL <transferId>\r\n");
        return;
    }
    
    std::string transferId = _params[0];
    
    DCCManager* dccManager = _server->getDCCManager();
    if (!dccManager) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC not available on this server\r\n");
        return;
    }
    
    DCCTransfer* transfer = dccManager->getTransfer(transferId);
    if (!transfer) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :Transfer not found\r\n");
        return;
    }
    
    // 権限チェック
    if (transfer->getSender() != _client && transfer->getReceiver() != _client) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :You are not authorized to cancel this transfer\r\n");
        return;
    }
    
    dccManager->cancelTransfer(transferId);
    _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                       " :DCC transfer cancelled (ID: " + transferId + ")\r\n");
}

// DCC STATUS コマンドの実装
DCCStatusCommand::DCCStatusCommand(Server* server, Client* client, const std::vector<std::string>& params)
    : Command(server, client, "DCC", params) {
    _requiresRegistration = true;
}

DCCStatusCommand::~DCCStatusCommand() {}

void DCCStatusCommand::execute() {
    if (!canExecute()) {
        _client->sendNumericReply(451, ":You have not registered");
        return;
    }
    
    DCCManager* dccManager = _server->getDCCManager();
    if (!dccManager) {
        _client->sendMessage(":server NOTICE " + _client->getNickname() + 
                           " :DCC not available on this server\r\n");
        return;
    }
    
    std::stringstream ss;
    ss << ":server NOTICE " << _client->getNickname() << " :=== DCC Status ===\r\n";
    ss << ":server NOTICE " << _client->getNickname() 
       << " :Active transfers: " << dccManager->getActiveTransferCount() << "\r\n";
    ss << ":server NOTICE " << _client->getNickname() 
       << " :Pending transfers: " << dccManager->getPendingTransferCount() << "\r\n";
    ss << ":server NOTICE " << _client->getNickname() 
       << " :Completed transfers: " << dccManager->getCompletedTransferCount() << "\r\n";
    
    unsigned long totalBytes = dccManager->getTotalBytesTransferred();
    if (totalBytes > 0) {
        ss << ":server NOTICE " << _client->getNickname() 
           << " :Total bytes transferred: ";
        
        if (totalBytes >= 1024 * 1024 * 1024) {
            ss << (double)totalBytes / (1024 * 1024 * 1024) << " GB";
        } else if (totalBytes >= 1024 * 1024) {
            ss << (double)totalBytes / (1024 * 1024) << " MB";
        } else if (totalBytes >= 1024) {
            ss << (double)totalBytes / 1024 << " KB";
        } else {
            ss << totalBytes << " B";
        }
        ss << "\r\n";
    }
    
    ss << ":server NOTICE " << _client->getNickname() << " :==================\r\n";
    
    _client->sendMessage(ss.str());
}
