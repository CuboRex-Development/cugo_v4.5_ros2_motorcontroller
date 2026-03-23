# cugo_v4.5_ros2_motorcontroller

ROS 2で CuGo V4.5 を制御するArduinoスケッチです。

ROS 2パッケージ [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) とセットで使用します。アプリケーションの全容や ROS PC 側の作業手順は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) を参照してください。

> [!WARNING]
> このArduinoスケッチは CuGo V4.5 専用です。
>
> クローラロボット開発プラットフォーム CuGo V4 / クローラロボット開発プラットフォーム V3i をご利用の方は [cugo_ros2_motorcontroller2](https://github.com/CuboRex-Development/cugo_ros2_motorcontroller2) を参照してください。


# Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Note](#note)
- [License](#license)


# Features

cugo_v4.5_ros2_motorcontroller は、ROS 2パッケージ `cugo_v4.5_ros2_control` から受信した目標速度指令をロボットに内蔵された車両コントローラ CRST01A に転送し、現在の走行速度をROS側に返答するインターフェースモジュールです。

上位PCとの通信トランスポートとして、USB-Serial と WiFi（外部ルータ経由） を選択できます。設定は `config.h` の 1行を変更するだけで切り替えられます。パケットの中身（COBSエンコード・プロトコル）はどちらのモードでも共通です。


# Requirements

**ハードウェア**
- Raspberry Pi Pico 2 WH（CuGo V4.5 に同梱）

**Arduinoライブラリ**
- [PacketSerial](https://github.com/bakercp/PacketSerial) — COBSエンコード/デコード
- [crst01a_arduino_lib](https://github.com/CuboRex-Development/crst01a_arduino_lib) — CRST01A 操作SDK（本リポジトリにサブモジュールとして含まれる）

**Arduino IDE ボード設定**
- ボード: Raspberry Pi Pico 2 / Pico 2W (Earle F. Philhower 版)


# Installation

1. 本リポジトリをサブモジュールごとクローンします。

   ```bash
   git clone --recurse-submodules <repository-url>
   ```

2. Arduino IDE のライブラリマネージャから `PacketSerial` をインストールします。

3. `crst01a_arduino_lib` を Arduino IDE のライブラリとして追加します。

4. `cugo_v4.5_ros2_motorcontroller/cugo_v4.5_ros2_motorcontroller.ino` を Arduino IDE で開きます。

5. CuGo V4.5 に搭載された基板の DIP スイッチを RPi モードに設定します。


# Configuration

編集するファイルは `config.h` のみです。それ以外のソースファイルを編集する必要はありません。

## USB-Serial モード（デフォルト）

追加設定は不要です。`config.h` の `USE_WIFI` 行がコメントアウトされていることを確認してください。

```cpp
// #define USE_WIFI   ← コメントアウトのまま (USB-Serial モード)
```

スケッチを書き込んだ後、USB ケーブルで Pico 2 WH と PC を接続してください。

| 接続先 | ケーブル | ポート |
|--------|----------|--------|
| PC（ROS 2） ↔ Pico 2 WH | USB ケーブル | USB（Pico の USB-C または Micro-USB 端子） |

その後の ROS PC 側の手順は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) を参照してください。

## WiFi モード（外部ルータ経由）

`config.h` の `USE_WIFI` のコメントを外し、接続先のWiFi情報を設定します。

```cpp
#define USE_WIFI
#define WIFI_SSID       "your_ssid"      // ← WiFiのSSID
#define WIFI_PASSWORD   "your_password"  // ← WiFiのパスワード
#define WIFI_TCP_PORT   (8080)           // ← TCPポート番号 (通常は変更不要)
```

IPアドレスを固定する場合は、`WIFI_STATIC_IP` のコメントも外して各アドレスを設定します。

```cpp
#define WIFI_STATIC_IP
#define WIFI_IP         IPAddress(192, 168, 0, 101)
#define WIFI_GATEWAY    IPAddress(192, 168, 0,   1)
#define WIFI_SUBNET     IPAddress(255, 255, 255,  0)
```

スケッチを書き込むと、Pico 2 WH は設定した SSID の WiFi ルータに接続します。

| 接続先 | 接続方法 |
|--------|----------|
| PC（ROS 2） ↔ WiFi ルータ | 有線 LAN またはWiFi |
| Pico 2 WH ↔ WiFi ルータ | WiFi（スケッチで設定した SSID に接続） |

> [!NOTE]
> Pico の IP アドレスは、ルータの管理画面で確認できます。`WIFI_DEBUG_SERIAL` を有効にすると USB シリアルモニタにも表示されます。

ROS PC 側の設定（socat の起動など）は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) を参照してください。


# Architecture

```
┌────────────────────────────────────────────────────────────┐
│                        PC (ROS 2)                          │
│              cugo_v4.5_ros2_control ノード                  │
└──────────────┬──────────────────────────┬──────────────────┘
               │ USB-Serial モード          │ WiFi モード
               │ USB CDC / PacketSerial     │ WiFi TCP / socat / PacketSerial
               │ 115200 bps                 │
┌──────────────▼──────────────────────────▼──────────────────┐
│           Raspberry Pi Pico 2 WH (インターフェースモジュール)  │
│            cugo_v4.5_ros2_motorcontroller                  │
└─────────────────────────┬──────────────────────────────────┘
                          │ UART0 (Serial1)
                          │ crst01a_arduino_lib
┌─────────────────────────▼──────────────────────────────────┐
│              CRST01A 車両コントローラ                         │
└────────────────────────────────────────────────────────────┘
```

**主要ファイル**

| ファイル | 役割 |
|---------|------|
| `config.h` | **ユーザー設定ファイル** — 通信モード・WiFi 設定（編集するのはこのファイルのみ） |
| `Transport.h / .cpp` | トランスポート抽象化 — USB-Serial / WiFi の切り替えを隠蔽 |
| `cugo_v4.5_ros2_motorcontroller.ino` | メインループ・フェイルセーフ・CRST01A 状態監視 |
| `RosComm.h / .cpp` | ROSとのパケット解析・送信 |
| `Crst01a.h / .cpp` | CRST01A 通信ラッパー |


# Protocol

パケット構成・データフォーマットの詳細は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) を参照してください。

**ハンドシェイク**

通信開始時、コントローラノードはイニシャル要求（ボディの先頭60バイトが全て0x00のパケット）を送信します。ロボット側はこれを受信するとCMDモードへ移行し、イニシャル応答を返します。

ハンドシェイクが完了するまで、速度指令は受け付けられません。

**フェイルセーフ**

100ms ごとに通信を監視し、5回連続（0.5秒間）パケットを受信しなかった場合は自動的に停止します。通信が回復した際はフェイルセーフ状態を解除し、再度ハンドシェイクを行います。


# Note

ご不明点がございましたら、[お問い合わせフォーム](https://cuborex.com/contact/)にてお問い合わせください。


# License

このプロジェクトはApache License 2.0のもと、公開されています。詳細はLICENSEをご覧ください。
