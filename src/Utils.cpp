#include "../include/Utils.hpp"

namespace Utils {
    // 文字列を指定の区切り文字で分割する
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;

        // 空文字列の場合は空のベクターを返す
        if (str.empty()) {
            return tokens;
        }

        // 分割後の要素数を制限（無限ループ防止）
        const size_t MAX_TOKENS = 100;

        std::stringstream ss(str);
        std::string token;
        size_t count = 0;

        while (std::getline(ss, token, delimiter) && count < MAX_TOKENS) {
            if (!token.empty()) {
                tokens.push_back(token);
                count++;
            }
        }

        // トークン数が多すぎる場合は警告
        if (count >= MAX_TOKENS) {
            std::cout << "\033[1;33m[WARNING] Too many tokens in split operation, truncating to "
                      << MAX_TOKENS << "\033[0m" << std::endl;
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
        // 空文字列チェック
        if (str.empty()) {
            return str;
        }

        // 極端に長い文字列のチェック
        if (str.length() > 1024) {
            std::cout << "\033[1;33m[WARNING] Very long string in toUpper operation ("
                      << str.length() << " chars)\033[0m" << std::endl;
        }

        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    // 文字列を小文字に変換する
    std::string toLower(const std::string& str) {
        // 空文字列チェック
        if (str.empty()) {
            return str;
        }

        // 極端に長い文字列のチェック
        if (str.length() > 1024) {
            std::cout << "\033[1;33m[WARNING] Very long string in toLower operation ("
                      << str.length() << " chars)\033[0m" << std::endl;
        }

        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    // 現在時刻を文字列として取得する
    std::string getCurrentTime() {
        time_t now = time(NULL);
        if (now == static_cast<time_t>(-1)) {
            std::cerr << "\033[1;31m[ERROR] Failed to get current time\033[0m" << std::endl;
            return "1970-01-01 00:00:00"; // エラー時のフォールバック
        }

        struct tm* tm_info = localtime(&now);
        if (tm_info == NULL) {
            std::cerr << "\033[1;31m[ERROR] Failed to convert time to local time\033[0m" << std::endl;
            return "1970-01-01 00:00:00"; // エラー時のフォールバック
        }

        char buffer[80];
        if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
            std::cerr << "\033[1;31m[ERROR] Failed to format time string\033[0m" << std::endl;
            return "1970-01-01 00:00:00"; // エラー時のフォールバック
        }

        return std::string(buffer);
    }

    // IRCレスポンスを整形する
    std::string formatResponse(int code, const std::string& target, const std::string& message) {
        // コードの範囲チェック（RFC 2812に準拠）
        if (code < 1 || code > 999) {
            std::cerr << "\033[1;31m[ERROR] Invalid response code: " << code << "\033[0m" << std::endl;
            code = 999; // エラー時のフォールバック
        }

        // ターゲットが空の場合のチェック
        std::string safeTarget = target;
        if (safeTarget.empty()) {
            std::cerr << "\033[1;31m[ERROR] Empty target in formatResponse\033[0m" << std::endl;
            safeTarget = "*"; // エラー時のフォールバック
        }

        // メッセージが長すぎる場合は切り詰める
        std::string safeMessage = message;
        if (safeMessage.length() > 400) {
            std::cerr << "\033[1;33m[WARNING] Response message too long, truncating\033[0m" << std::endl;
            safeMessage = safeMessage.substr(0, 400);
        }

        std::stringstream ss;

        // コードを3桁の0埋め文字列に変換
        ss << ":" << IRC_SERVER_NAME << " " << std::setfill('0') << std::setw(3) << code << " " << safeTarget << " " << safeMessage << "\r\n";

        return ss.str();
    }

    // 明示的なtoString実装例（テンプレート版のほかに、特定の型向けの実装を追加できる）
    // 例: 時間の整形
    std::string formatDuration(time_t seconds) {
        // 負の値のチェック
        if (seconds < 0) {
            std::cerr << "\033[1;31m[ERROR] Negative duration value: " << seconds << "\033[0m" << std::endl;
            seconds = 0; // エラー時のフォールバック
        }

        // 極端に大きい値のチェック
        if (seconds > 31536000) { // 1年以上
            std::cerr << "\033[1;33m[WARNING] Very large duration value: " << seconds << " seconds\033[0m" << std::endl;
        }

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