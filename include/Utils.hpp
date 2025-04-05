#ifndef UTILS_HPP
# define UTILS_HPP

# include <iostream>
# include <string>
# include <cstring>
# include <cstdlib>
# include <cerrno>
# include <vector>
# include <map>
# include <algorithm>
# include <sstream>
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>
# include <fcntl.h>
# include <poll.h>
# include <signal.h>
# include <ctime>
# include <iomanip> // std::setfill, std::setw

// IRC定数
# define IRC_SERVER_NAME "ft_irc"
# define IRC_VERSION "1.0"
# define IRC_CREATION_DATE "2025-03-28"
# define MAX_CLIENTS 100
# define BUFFER_SIZE 1024
# define MAX_CHANNELS 100
# define CHANNEL_PREFIX '#'

// レスポンスコード
// - エラーコード
# define ERR_NOSUCHNICK 401
# define ERR_NOSUCHSERVER 402
# define ERR_NOSUCHCHANNEL 403
# define ERR_CANNOTSENDTOCHAN 404
# define ERR_TOOMANYCHANNELS 405
# define ERR_TOOMANYTARGETS 407
# define ERR_NOORIGIN 409
# define ERR_NORECIPIENT 411
# define ERR_NOTEXTTOSEND 412
# define ERR_NONICKNAMEGIVEN 431
# define ERR_ERRONEUSNICKNAME 432
# define ERR_NICKNAMEINUSE 433
# define ERR_NICKCOLLISION 436
# define ERR_USERNOTINCHANNEL 441
# define ERR_NOTONCHANNEL 442
# define ERR_USERONCHANNEL 443
# define ERR_NOTREGISTERED 451
# define ERR_NEEDMOREPARAMS 461
# define ERR_ALREADYREGISTRED 462
# define ERR_PASSWDMISMATCH 464
# define ERR_KEYSET 467
# define ERR_CHANNELISFULL 471
# define ERR_UNKNOWNMODE 472
# define ERR_INVITEONLYCHAN 473
# define ERR_BANNEDFROMCHAN 474
# define ERR_BADCHANNELKEY 475
# define ERR_BADCHANMASK 476
# define ERR_CHANOPRIVSNEEDED 482
# define ERR_UMODEUNKNOWNFLAG 501
# define ERR_USERSDONTMATCH 502

// - リプライコード
# define RPL_WELCOME 001
# define RPL_YOURHOST 002
# define RPL_CREATED 003
# define RPL_MYINFO 004
# define RPL_UMODEIS 221
# define RPL_AWAY 301
# define RPL_UNAWAY 305
# define RPL_NOWAWAY 306
# define RPL_CHANNELMODEIS 324
# define RPL_NOTOPIC 331
# define RPL_TOPIC 332
# define RPL_INVITING 341
# define RPL_NAMREPLY 353
# define RPL_ENDOFNAMES 366
# define RPL_MOTDSTART 375
# define RPL_MOTD 372
# define RPL_ENDOFMOTD 376

// ユーティリティ関数
namespace Utils {
    // 文字列分割関数
    std::vector<std::string> split(const std::string& str, char delimiter);

    // トリミング関数
    std::string trim(const std::string& str);

    // 大文字変換
    std::string toUpper(const std::string& str);

    // 小文字変換
    std::string toLower(const std::string& str);

    // 現在時刻取得
    std::string getCurrentTime();

    // レスポンス整形
    std::string formatResponse(int code, const std::string& target, const std::string& message);
}

#endif
