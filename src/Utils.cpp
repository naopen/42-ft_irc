#include "../include/Utils.hpp"

namespace Utils {
    // 文字列を指定の区切り文字で分割する
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;

        while (std::getline(ss, token, delimiter)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }

        return tokens;
    }

    // 文字列の前後の空白を削除する
    std::string trim(const std::string& str) {
        if (str.empty()) {
            return str;
        }

        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) {
            return "";
        }

        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(first, last - first + 1);
    }

    // 文字列を大文字に変換する
    std::string toUpper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    // 文字列を小文字に変換する
    std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // 現在時刻を文字列として取得する
    std::string getCurrentTime() {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);

        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);

        return std::string(buffer);
    }

    // IRCレスポンスを整形する
    std::string formatResponse(int code, const std::string& target, const std::string& message) {
        std::stringstream ss;

        // コードを3桁の0埋め文字列に変換
        ss << ":" << IRC_SERVER_NAME << " " << std::setfill('0') << std::setw(3) << code << " " << target << " " << message << "\r\n";

        return ss.str();
    }

    // 明示的なtoString実装例（テンプレート版のほかに、特定の型向けの実装を追加できる）
    // 例: 時間の整形
    std::string formatDuration(time_t seconds) {
        std::stringstream ss;

        int days = seconds / 86400;
        seconds %= 86400;
        int hours = seconds / 3600;
        seconds %= 3600;
        int minutes = seconds / 60;
        seconds %= 60;

        if (days > 0) {
            ss << days << "d ";
        }

        if (hours > 0 || days > 0) {
            ss << hours << "h ";
        }

        if (minutes > 0 || hours > 0 || days > 0) {
            ss << minutes << "m ";
        }

        ss << seconds << "s";

        return ss.str();
    }
}