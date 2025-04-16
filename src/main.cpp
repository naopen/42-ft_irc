#include "../include/Server.hpp"
#include <cstring>
#include <cctype>

// ポート番号が有効な数値かチェックする関数
bool isValidPort(const char* str) {
    // 空の文字列かチェック
    if (!str || *str == '\0') {
        std::cout << "Error: Empty port number provided." << std::endl;
        return false;
    }

    // 文字列長の制限チェック
    size_t len = strlen(str);
    if (len > 10) { // 最大10桁（厳密には65535は5桁だが余裕を持たせる）
        std::cout << "Error: Port number is too long." << std::endl;
        return false;
    }

    // 先頭が0で、かつ文字列長が1より大きい場合は不正
    // (ポート0自体は有効だが、01, 001などは不正)
    if (*str == '0' && *(str + 1) != '\0') {
        std::cout << "Error: Port number cannot have leading zeros." << std::endl;
        return false;
    }

    // すべての文字が数字かチェック
    for (size_t i = 0; i < len; ++i) {
        if (!isdigit(str[i])) {
            std::cout << "Error: Port must contain only digits." << std::endl;
            return false;
        }
    }

    // オーバーフローチェック（文字列が大きすぎてintに収まらない場合）
    long port = strtol(str, NULL, 10);
    if (port == LONG_MAX || port == LONG_MIN || port > 65535) {
        std::cout << "Error: Port number out of range. Must be between 1 and 65535." << std::endl;
        return false;
    }

    // 数値の範囲チェックはatoi後に行う
    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    // ポート番号の文字列検証
    if (!isValidPort(argv[1])) {
        std::cout << "Invalid port format. Port must be a number between 1 and 65535 with no leading zeros." << std::endl;
        return 1;
    }

    // ポート番号の範囲チェック
    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cout << "Invalid port number. Port must be between 1 and 65535." << std::endl;
        return 1;
    }

    // パスワードのチェック
    std::string password = argv[2];
    if (password.empty()) {
        std::cout << "Password cannot be empty." << std::endl;
        return 1;
    }

    try {
        // サーバーの作成と起動
        Server server(port, password);
        server.setup();
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
