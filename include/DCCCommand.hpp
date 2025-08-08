#ifndef DCCCOMMAND_HPP
# define DCCCOMMAND_HPP

# include "Command.hpp"
# include "DCCManager.hpp"

// DCC SEND コマンド
class DCCSendCommand : public Command {
private:
    std::string     parseFilename(const std::string& path) const;
    unsigned long   getFileSize(const std::string& filepath) const;
    bool            validateFilepath(const std::string& filepath) const;
    std::string     convertIPToLong(const std::string& ip) const;
    
public:
    DCCSendCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~DCCSendCommand();
    
    void execute();
};

// DCC GET/ACCEPT コマンド
class DCCGetCommand : public Command {
private:
    bool            parseTransferInfo(const std::string& info, std::string& transferId);
    
public:
    DCCGetCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~DCCGetCommand();
    
    void execute();
};

// DCC REJECT コマンド
class DCCRejectCommand : public Command {
public:
    DCCRejectCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~DCCRejectCommand();
    
    void execute();
};

// DCC LIST コマンド
class DCCListCommand : public Command {
public:
    DCCListCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~DCCListCommand();
    
    void execute();
};

// DCC CANCEL コマンド
class DCCCancelCommand : public Command {
public:
    DCCCancelCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~DCCCancelCommand();
    
    void execute();
};

// DCC STATUS コマンド
class DCCStatusCommand : public Command {
public:
    DCCStatusCommand(Server* server, Client* client, const std::vector<std::string>& params);
    ~DCCStatusCommand();
    
    void execute();
};

#endif
