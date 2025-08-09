# IRC Server Bonus Features

## Bot System

### JankenBot (Rock-Paper-Scissors Bot)

JankenBotは、IRCサーバーに統合されたじゃんけんゲームBotです。

#### 使用方法

1. **サーバーの起動**
```bash
./ircserv <port> <password>
```

2. **IRCクライアントで接続**
```bash
# 例: irssi, hexchat, nc など
nc localhost <port>
```

3. **認証とニックネーム設定**
```
PASS <password>
NICK <your_nickname>
USER <username> 0 * :Real Name
```

4. **JankenBotとゲーム**

JankenBotにプライベートメッセージを送信してゲームを開始します：

```
PRIVMSG JankenBot :help
PRIVMSG JankenBot :start
PRIVMSG JankenBot :rock
```

#### コマンド一覧

| コマンド | 説明 |
|---------|------|
| `help` | ヘルプメッセージを表示 |
| `start` / `play` | 新しいゲームを開始 |
| `rock` / `r` | グー（Rock）を出す |
| `scissors` / `s` | チョキ（Scissors）を出す |
| `paper` / `p` | パー（Paper）を出す |
| `stats` / `score` | 現在のスコアと統計を表示 |
| `reset` / `quit` | ゲームをリセット |

#### チャンネルでの使用

チャンネル内でBotに言及することもできます：

```
JOIN #general
PRIVMSG #general :JankenBot: help
PRIVMSG #general :!janken
```

### Bot開発ガイド

新しいBotを追加する場合：

1. `Bot`基底クラスを継承
2. 純粋仮想関数を実装:
   - `onMessage()`
   - `onPrivateMessage()`
   - `onChannelMessage()`
3. `BotManager::initializeBots()`に追加

#### 例: 新しいBotの追加

```cpp
class MyBot : public Bot {
public:
    MyBot(Server* server) : Bot(server, "MyBot", "My Custom Bot") {}
    
    void onPrivateMessage(Client* sender, const std::string& message) {
        // 実装
    }
    
    void onChannelMessage(Client* sender, const std::string& channel, const std::string& message) {
        // 実装
    }
    
    void onMessage(Client* sender, const std::string& target, const std::string& message) {
        // 実装
    }
};
```

## DCC File Transfer (Direct Client-to-Client)

DCCファイル転送機能により、IRCクライアント間で直接ファイルを転送できます。

### 使用方法

#### シナリオ1: 送信者主導の転送（Push型）

##### 1. ファイル送信

```
DCC SEND <nickname> <filepath>
```

例:
```
DCC SEND Alice ./dcc_transfers/test_file.txt
```

##### 2. ファイル受信

送信リクエストを受け取ったら、以下のコマンドで承認:

```
DCC GET <transferId>
DCC ACCEPT <transferId>
```

または、送信者とファイル名を指定して受信:

```
DCC GET <nickname> <filename>
```

例:
```
DCC GET Alice test_file.txt
```

リクエストを拒否する場合:

```
DCC REJECT <transferId>
```

**注意:** `DCC GET <nickname> <filename>` 形式を使用した場合、保留中の転送が存在しない場合は、指定したニックネームにファイルリクエストを送信します。相手がDCC SENDで応答した場合、転送は自動的に承認されます。

#### シナリオ2: 受信者主導の転送（Pull型）

##### 1. ファイルリクエスト

受信者がファイルを要求:

```
DCC GET <nickname> <filename>
```

例:
```
# BobがAliceからtest_file.txtを要求
DCC GET Alice test_file.txt
```

##### 2. 送信者の応答

送信者はGETリクエストを受け取り、ファイルを送信:

```
DCC SEND <nickname> <filepath>
```

##### 3. 自動承認

Pull型の転送では、リクエスト者がファイルを自動的に承認します。

#### 3. 転送管理コマンド

| コマンド | 説明 |
|---------|------|
| `DCC LIST` | アクティブな転送の一覧表示 |
| `DCC STATUS` | 転送統計情報を表示 |
| `DCC CANCEL <transferId>` | 転送をキャンセル |

### 機能特徴

- **直接転送**: クライアント間で直接データを転送
- **進捗表示**: 転送の進捗をリアルタイムで表示
- **転送速度表示**: 転送速度をMB/s、KB/s単位で表示
- **タイムアウト機能**: 5分間アクティビティがない場合は自動キャンセル
- **ファイルサイズ制限**: 最大100MBまでのファイルをサポート
- **同時転送制限**: 1クライアントあたり最大3つまで
- **ポート管理**: 5000-5100のポート範囲で自動割り当て

### セキュリティ

- 転送ファイルは`./dcc_transfers/`ディレクトリに制限
- 受信ファイルは`./dcc_transfers/received/`に保存
- ファイルサイズ検証
- パストラバーサル攻撃の防止

### テスト例

```bash
# サーバー起動
./ircserv 6667 password123

# クライアント1（送信者）
nc localhost 6667
PASS password123
NICK Alice
USER alice 0 * :Alice User

# クライアント2（受信者）
nc localhost 6667  
PASS password123
NICK Bob
USER bob 0 * :Bob User

# AliceからBobにファイル送信
# Alice:
DCC SEND Bob ./dcc_transfers/test_file.txt

# Bobに通知が届く
# Bob:
DCC LIST  # ペンディング転送を確認
DCC ACCEPT <transferId>  # 転送を承認

# 両方のクライアントで進捗を確認
DCC STATUS
```

## ビルド方法

```bash
make        # ビルド
make clean  # オブジェクトファイル削除
make fclean # 完全クリーン
make re     # リビルド
```

## テスト方法

### 基本的なテスト

```bash
# サーバー起動
./ircserv 6667 password123

# 別ターミナルでクライアント接続
nc localhost 6667

# 認証
PASS password123
NICK TestUser
USER testuser 0 * :Test User

# JankenBotとゲーム
PRIVMSG JankenBot :start
PRIVMSG JankenBot :rock
```

### 複数クライアントでのテスト

1. 複数のターミナルでクライアントを起動
2. チャンネルに参加して相互作用をテスト
3. BotがチャンネルJOINイベントに反応することを確認

## 評価基準チェックリスト

### Mandatory Part
- [x] poll()は1つのみ使用
- [x] fcntl()は`O_NONBLOCK`設定のみに使用
- [x] 複数接続の同時処理
- [x] ノンブロッキング動作
- [x] 部分的なコマンド送信の処理
- [x] クライアントの予期しない切断処理

### Bonus Part
- [x] IRC Bot実装
  - [x] プライベートメッセージ対応
  - [x] チャンネルメッセージ対応
  - [x] インタラクティブな機能（じゃんけんゲーム）
- [x] ファイル転送機能 (DCC)
  - [x] DCC SEND/GETコマンド
  - [x] ファイル転送の進捗表示
  - [x] 転送管理機能
  - [x] セキュリティ対策

## トラブルシューティング

### Botが応答しない場合
1. サーバーログを確認
2. Botのニックネームが正しいか確認（JankenBot）
3. PRIVMSGコマンドの形式を確認

### コンパイルエラー
1. C++98標準を使用していることを確認
2. 必要なヘッダーファイルがインクルードされているか確認
3. `make clean && make`でリビルド

## 今後の機能追加予定

- [ ] FTPサーバー実装
- [ ] 追加のBot（クイズBot、天気Bot等）
- [ ] Bot設定ファイル対応
- [ ] Bot永続化機能
