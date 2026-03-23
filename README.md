# cugo_v4.5_ros2_motorcontroller

ROS 2で CuGo V4.5 を制御するArduinoスケッチです。

ROS 2パッケージ [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) とセットで使用します。アプリケーションの全容や ROS PC 側の作業手順は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) を参照してください。

### 対応製品

<!-- TODO: V4.5のリンクを貼る -->
* [CuGo V4.5](null)

> [!WARNING]
> クローラロボット開発プラットフォーム CuGo V4 / クローラロボット開発プラットフォーム V3i をご利用の方は [cugo_ros2_motorcontroller2](https://github.com/CuboRex-Development/cugo_ros2_motorcontroller2) を参照してください。


# Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Protocol](#protocol)
- [Note](#note)
- [License](#license)


# Features

cugo_v4.5_ros2_motorcontroller は、ROS 2パッケージ [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) から受信した目標速度指令をロボットに内蔵された車両コントローラ CRST01A に転送し、現在の走行速度をROS側に返答するインターフェースモジュールです。

本プログラムを実行しているCuGo V4.5とROS 2 PCとの通信方式は USB-Serial をデフォルトとしていますが、その他の通信方式も選択することができます。対応している通信の詳細は[cugo_v4.5_ros2_controlのreadme](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) の Connection をご参照ください。

<img width="4077" height="2541" alt="cugo_v4 5_ros2_motorcontroller" src="https://github.com/user-attachments/assets/da7c65b4-b58b-493c-92b3-0094dca43b07" />

# Requirements

### ハードウェア
- Raspberry Pi Pico 2 WH（CuGo V4.5 に同梱）

### Arduinoライブラリ
- [PacketSerial](https://github.com/bakercp/PacketSerial) — COBSエンコード/デコード
- [crst01a_arduino_lib](https://github.com/CuboRex-Development/crst01a_arduino_lib) — CRST01A 操作SDK（本リポジトリにサブモジュールとして含まれる）

### Arduino IDE ボード設定
- ボード: Raspberry Pi Pico 2 / Pico 2W (Earle F. Philhower 版)


# Installation

1. 本リポジトリをサブモジュールごとクローンします。

   **ブラウザから**：  
   [本リポジトリのページ](https://github.com/CuboRex-Development/cugo_v4.5_ros2_motorcontroller/)上部の「code」ボタンをクリックし、「Download ZIP」をおしてください。
   また、[crst01a_arduino_lib](https://github.com/CuboRex-Development/crst01a_arduino_lib)も同様にダウンロードしてください。


   **コマンドラインから**：  
   以下のコマンドを実行してください。  

   ```bash
   git clone --recurse-submodules <repository-url>
   ```
2. [arduino-pico](https://github.com/earlephilhower/arduino-pico)を参照し、Raspberry Pi Pico/RP2040/RP2350 をボードマネージャに追加します。


3. Arduino IDE のライブラリマネージャから `PacketSerial` をインストールします。

4. `crst01a_arduino_lib` を Arduino IDE のライブラリとして追加します。

5. CuGo V4.5 に搭載された基板の DIP スイッチを RPi モードに設定します。
<!-- TODO:DIPスイッチの画像を追加する -->

6. `cugo_v4.5_ros2_motorcontroller/cugo_v4.5_ros2_motorcontroller.ino` を Arduino IDE で開きます。

7. [Configuration](#configuration)に沿い、必要に応じてプログラムの設定を変更します。

8.  Arduino IDE を開いたPCとCuGo V4.5 に搭載されたRaspberry Pi Pico 2 WH をUSBケーブルで接続し、プログラムを書き込みます。


以上でRaspberry Pi Pico側の手順は完了となります。引き続き、[cugo_v4.5_ros2_controlのreadme](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control)のInstallation、 Usage の手順を実施してください。

# Configuration

ROS 2 PC との接続方法に合わせて、プログラムの設定を変更します。

編集するファイルは `config.h` のみです。それ以外のソースファイルを編集する必要はありません。
接続方法の詳細は[cugo_v4.5_ros2_controlのreadme](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) の Connection の章をご参照ください。

### USB-Serial 接続（デフォルト）

追加設定は不要です。`config.h` の `USE_WIFI` 行がコメントアウトされていることを確認してください。

```cpp
// #define USE_WIFI   ← コメントアウトのまま (USB-Serial 接続)
```

### WiFi 接続（外部ルータ経由）

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

スケッチを書き込むと、Raspberry Pi Pico 2 WH は設定した SSID の WiFi ルータに接続します。

> [!NOTE]
> Pico の IP アドレスは、ルータの管理画面で確認できます。`WIFI_DEBUG_SERIAL` を有効にすると USB シリアルモニタにも表示されます。

# Protocol

パケット構成・データフォーマットの詳細は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) の PROTOCOL.md を参照してください。

### ハンドシェイク

通信開始時、コントローラノードはイニシャル要求（ボディの先頭60バイトが全て0x00のパケット）を送信します。ロボット側はこれを受信するとCMDモードへ移行し、イニシャル応答を返します。

ハンドシェイクが完了するまで、速度指令は受け付けられません。

### フェイルセーフ

100ms ごとに通信を監視し、5回連続（0.5秒間）パケットを受信しなかった場合は自動的に停止します。通信が回復した際はフェイルセーフ状態を解除し、再度ハンドシェイクを行います。


# Note

ご不明点がございましたら、[お問い合わせフォーム](https://cuborex.com/contact/)にてお問い合わせください。


# License

このプロジェクトはApache License 2.0のもと、公開されています。詳細はLICENSEをご覧ください。
