#include "../include/Server.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        return 1;
    }

    // ポート番号のチェック
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
