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
- [Debug](#debug)
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
- IP/Bluetooth Stack: Bluetoothモード使用時のみ `IPv4 + Bluetooth` を選択（後述）


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

5. `cugo_v4.5_ros2_motorcontroller/cugo_v4.5_ros2_motorcontroller.ino` を Arduino IDE で開きます。

6. [Configuration](#configuration)に沿い、必要に応じてプログラムの設定を変更します。

7.  Arduino IDE を開いたPCとCuGo V4.5 に搭載されたRaspberry Pi Pico 2 WH をUSBケーブルで接続し、プログラムを書き込みます。


以上でRaspberry Pi Pico側の手順は完了となります。引き続き、[cugo_v4.5_ros2_controlのreadme](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control)のInstallation、 Usage の手順を実施してください。

# Configuration

ROS 2 PC との接続方法に合わせて、プログラムの設定を変更します。

編集するファイルは `config.h` のみです。それ以外のソースファイルを編集する必要はありません。
接続方法の詳細は[cugo_v4.5_ros2_controlのreadme](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) の Connection の章をご参照ください。

### USB-Serial 接続（デフォルト）

追加設定は不要です。`config.h` の `USE_BOX_CN`・`USE_WIFI`・`USE_BLUETOOTH` 行がコメントアウトされていることを確認してください。

```cpp
// #define USE_BOX_CN    ← コメントアウトのまま
// #define USE_BLUETOOTH ← コメントアウトのまま
// #define USE_WIFI      ← コメントアウトのまま
// #define WIFI_AP_MODE  ← コメントアウトのまま
```

### BOXコネクタ-Serial接続

基板上のボックスコネクタに配線を接続し、Raspberry Pi Pico 2 WH のUARTピンを使用するモードです。UART1（GP8: TX / GP9: RX）を使用してROS 2 PC と通信します。USB-Serial 接続と同じプロトコルで動作します。

使用するためには、`config.h` の `USE_BOX_CN` のコメントを外します。

```cpp
#define USE_BOX_CN    // ← コメントを外す
// #define USE_BLUETOOTH ← コメントアウトのまま
// #define USE_WIFI      ← コメントアウトのまま
// #define WIFI_AP_MODE  ← コメントアウトのまま

```

ボックスコネクタの UART TX/RX ピン（GP8: TX / GP9: RX）を ROS 2 PC のシリアルポートに接続してください。

### Bluetooth モード

Raspberry Pi Pico 2 WH の Classic Bluetooth（SPP：Serial Port Profile）を使用してROS 2 PCと通信します。PC側では仮想シリアルポートとして認識されるため、ペアリング後はほかのシリアルモードと同様に扱えます。

> [!IMPORTANT]
> Arduino IDE のBluetoothスタック設定が必要です。
> `USE_BLUETOOTH` を有効にしてビルドする前に、Arduino IDE の `ツール → IP/Bluetooth Stack` を `IPv4 + Bluetooth` に変更してください。
> デフォルトの `IPv4 Only` のままではコンパイルエラーになります。

<img width="553" height="810" alt="Bluetooth_stack説明" src="https://github.com/user-attachments/assets/0bb74cc9-5901-4685-9f08-9a7533b4c2ee" />


使用するためには、`config.h` の `USE_BLUETOOTH` のコメントを外します。

```cpp
// #define USE_BOX_CN    ← コメントアウトのまま
#define USE_BLUETOOTH // ← コメントを外す
// #define USE_WIFI      ← コメントアウトのまま
// #define WIFI_AP_MODE  ← コメントアウトのまま

```

デバイス名（ペアリング時に表示される名前）は `BT_DEVICE_NAME` で変更できます。

```cpp
#define BT_DEVICE_NAME  "CuGo_BT"   // ← 任意の名前に変更可能
```

スケッチを書き込むと、Raspberry Pi Pico 2 WH が Bluetooth デバイスとして起動します。`INFO_SERIAL` が有効な場合、USB シリアルモニタに以下のように表示されます。

```text
Mode: Bluetoothモード
コントローラ起動中..... 完了
Device Name   : CuGo_BT
BT MAC Address: AB:CD:EF:12:34:56
ペアリング待機中...
```

ペアリング中は PIN コードが表示されます。ペアリング・接続・切断のイベントもシリアルモニタに出力されます。

```text
ペアリング要求を受けました。接続する機器に表示されたPINが一致しているか確認してください。
ペアリングPIN: 123456
ペアリング完了
ペアリング済みの機器と接続しました (AB:CD:EF:12:34:56)
切断しました (reason=0x13)
```

> [!NOTE]
> MACアドレスはROS PC側のペアリング時に使用します。不要な場合は `config.h` の `INFO_SERIAL` 行をコメントアウトしてください。

### WiFi APモード

Raspberry Pi Pico 2 WH 自身をアクセスポイントとして動作させます。WiFiルータは不要です。

使用するためには、`config.h` の `USE_WIFI` と `WIFI_AP_MODE` の両方のコメントを外し、アクセスポイントの SSID とパスワードを設定します。

```cpp
// #define USE_BOX_CN    ← コメントアウトのまま
// #define USE_BLUETOOTH ← コメントアウトのまま
#define USE_WIFI      // ← コメントを外す
#define WIFI_AP_MODE  // ← コメントを外す
```


```cpp
// ------------------------------------------------------------
// WiFi APモード設定 (USE_WIFI かつ WIFI_AP_MODE 定義時のみ有効)
// ------------------------------------------------------------
#if defined(USE_WIFI) && defined(WIFI_AP_MODE)

#define WIFI_AP_SSID        "CuGo_AP"    // ← アクセスポイントのSSID
#define WIFI_AP_PASSWORD    "cugo1234"   // ← アクセスポイントのパスワード
#define WIFI_TCP_PORT       (8080)       // ← TCPポート番号 (通常は変更不要)
#define WIFI_AP_CHANNEL     (1)          // ← チャンネル番号 (1 / 6 / 11 を推奨)
```

スケッチを書き込むと、Raspberry Pi Pico 2 WH がアクセスポイントとして起動します。ROS PC をそのアクセスポイントに接続し、Pico の IP アドレス（デフォルト: `192.168.42.1`）を ROS 側の接続先として指定してください。

> [!TIP]
> 周辺の WiFi 機器との電波干渉により通信が不安定になる場合は、`WIFI_AP_CHANNEL` を変更してください。

<!-- -->

> [!NOTE]
> 起動時に USB シリアルモニタへモード名と IP アドレスが表示されます。不要な場合は `config.h` の `INFO_SERIAL` 行をコメントアウトしてください。

### WiFi Stationモード

Raspberry Pi Pico 2 WH とROS 2 PC 外部のWiFiルータに接続して動作させます。WiFiルータはご自身でご用意ください。

使用するためには、`config.h` の `USE_WIFI` のコメントを外し、接続先のWiFi情報を設定します。

```cpp
// #define USE_BOX_CN    ← コメントアウトのまま
// #define USE_BLUETOOTH ← コメントアウトのまま
#define USE_WIFI      // ← コメントを外す
// #define WIFI_AP_MODE  ← コメントアウトのまま
```

```cpp
// ------------------------------------------------------------
// WiFi Stationモード設定 (USE_WIFI 定義かつ WIFI_AP_MODE 未定義時のみ有効)
// ------------------------------------------------------------
#elif defined(USE_WIFI)

#define WIFI_SSID       "your_ssid"      // ← WiFiのSSID
#define WIFI_PASSWORD   "your_password"  // ← WiFiのパスワード
#define WIFI_TCP_PORT   (8080)           // ← TCPポート番号 (通常は変更不要)
```

IPアドレスを固定する場合は、`WIFI_STATIC_IP` のコメントも外して各アドレスを設定します。

```cpp
// 静的IPを使用する場合はコメントを外して各アドレスを設定してください
#define WIFI_STATIC_IP
#ifdef WIFI_STATIC_IP
#define WIFI_IP         IPAddress(192, 168,  0, 101)
#define WIFI_GATEWAY    IPAddress(192, 168,  0,   1)
#define WIFI_SUBNET     IPAddress(255, 255, 255,  0)
#endif // WIFI_STATIC_IP
```

スケッチを書き込むと、Raspberry Pi Pico 2 WH は設定した SSID の WiFi ルータに接続します。

> [!NOTE]
> 起動時に USB シリアルモニタへモード名と IP アドレスが表示されます。不要な場合は `config.h` の `INFO_SERIAL` 行をコメントアウトしてください。ルータの管理画面でも IP アドレスを確認できます。

# Protocol

パケット構成・データフォーマットの詳細は [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) の PROTOCOL.md を参照してください。

### ハンドシェイク

通信開始時、コントローラノードはイニシャル要求（ボディの先頭60バイトが全て0x00のパケット）を送信します。ロボット側はこれを受信するとCMDモードへ移行し、イニシャル応答を返します。

ハンドシェイクが完了するまで、速度指令は受け付けられません。

### フェイルセーフ

100ms ごとに通信を監視し、5回連続（0.5秒間）パケットを受信しなかった場合は自動的に停止します。通信が回復した際はフェイルセーフ状態を解除し、再度ハンドシェイクを行います。


# Debug

> [!NOTE]
> このセクションは開発者向けの情報です。通常の使用では変更不要です。

USB-Serial 接続以外の接続方法を使用している際は、USBシリアルポート経由でプログラムのステータスやログを確認することが可能です。
有効化するためには、`config.h` に定義されたデバッグフラグの、コメントを外してください。


### BOXコネクタ-Serial モード

| フラグ                    | 出力内容                                           |
| ------------------------- | -------------------------------------------------- |
| `DEBUG_BOX_CN_TX_LOG`     | COBSエンコード前の送信データ（生パケット）         |
| `DEBUG_BOX_CN_RX_LOG`     | COBSデコード後の受信データ（生パケット）           |
| `DEBUG_BOX_CN_TX_RAW_LOG` | COBSエンコード後の送信データ（ワイヤ上のバイト列） |
| `DEBUG_BOX_CN_RX_RAW_LOG` | COBSデコード前の受信データ（ワイヤ上のバイト列）   |


### Bluetooth モード

| フラグ                | 出力内容                                                                                    |
| --------------------- | ------------------------------------------------------------------------------------------- |
| `INFO_SERIAL`         | 起動時のモード名・デバイス名・MACアドレス、ペアリング PIN、接続・切断イベント（デフォルト有効） |
| `DEBUG_BT_TX_LOG`     | COBSエンコード前の送信データ（生パケット）                                                  |
| `DEBUG_BT_RX_LOG`     | COBSデコード後の受信データ（生パケット）                                                    |
| `DEBUG_BT_TX_RAW_LOG` | COBSエンコード後の送信データ（ワイヤ上のバイト列）                                          |
| `DEBUG_BT_RX_RAW_LOG` | COBSデコード前の受信データ（ワイヤ上のバイト列）                                            |

`INFO_SERIAL` 有効時に出力される切断理由コード（`reason`）の主な値は以下のとおりです。

| reason コード | 意味                                                 |
| ------------- | ---------------------------------------------------- |
| `0x08`        | Connection Timeout（リンク監視タイムアウト）         |
| `0x13`        | Remote User Terminated（接続先が切断）               |
| `0x16`        | Connection Terminated by Local Host（Pico 側が切断） |

### WiFi APモード / WiFi Stationモード
 `config.h` に以下のデバッグフラグを用意しています。コメントを外すと USB シリアルモニタに対応するログが出力されます。

| フラグ                  | 出力内容                                           |
| ----------------------- | -------------------------------------------------- |
| `INFO_SERIAL`           | 起動時のモード名・IP アドレス（デフォルト有効）    |
| `DEBUG_SERIAL_STATS`    | 1秒ごとのループ統計（後述）                        |
| `DEBUG_WIFI_TX_LOG`     | COBSエンコード前の送信データ（生パケット）         |
| `DEBUG_WIFI_RX_LOG`     | COBSデコード後の受信データ（生パケット）           |
| `DEBUG_WIFI_TX_RAW_LOG` | COBSエンコード後の送信データ（ワイヤ上のバイト列） |
| `DEBUG_WIFI_RX_RAW_LOG` | COBSデコード前の受信データ（ワイヤ上のバイト列）   |


#### DEBUG_SERIAL_STATS の出力形式

1秒ごとに以下の形式でループ統計を出力します。

```text
[STATS] loops/s=1000  maxLoop=2500us  maxUpdate=650us  maxDelay1=1500us  maxAvail=74B
```

| フィールド  | 内容                                                                                 |
| ----------- | ------------------------------------------------------------------------------------ |
| `loops/s`   | 1秒間のループ実行回数。CPU飽和やブロッキングの有無を示す                             |
| `maxLoop`   | 1ループあたりの最大所要時間 [µs]。この値が大きいほど特定のループで処理が詰まっている |
| `maxUpdate` | `transport.update()` の最大所要時間 [µs]。WiFi TCP受信処理の負荷                     |
| `maxDelay1` | `delay(1)` の実際の最大所要時間 [µs]。CYW43 WiFiスタックのバックグラウンド処理コスト |
| `maxAvail`  | `update()` 呼び出し直前の TCP 受信バッファの最大蓄積バイト数。1パケット = 74B        |

各値は1秒ごとにリセットされるウィンドウ内の最大値です。デバッグの際は、以下の情報を参考にしてください。


- `maxUpdate` が大きい → WiFi TCP受信処理が重い（パケット受信時のみ増加する）
- `maxDelay1` が大きい → CYW43スタックがARP/TCPの管理処理を実施している
- `loops/s` が極端に低い → ループ全体がブロッキングされている
- `maxAvail` が 74B 以上 → パケットが蓄積されている。74B の倍数でパケット数がわかる（148B = 2パケット、222B = 3パケット、...）


# Note

ご不明点がございましたら、[お問い合わせフォーム](https://cuborex.com/contact/)にてお問い合わせください。


# License

このプロジェクトはApache License 2.0のもと、公開されています。詳細はLICENSEをご覧ください。
