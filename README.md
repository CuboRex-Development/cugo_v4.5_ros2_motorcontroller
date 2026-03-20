# cugo_v4.5_ros2_motorcontroller

ROS 2で CuGo V4.5 を制御するArduinoスケッチです。

セットでROS 2パッケージの [cugo_v4.5_ros2_control](https://github.com/CuboRex-Development/cugo_v4.5_ros2_control) と使用します。

> [!WARNING]
> このArduinoスケッチは CuGo V4.5 専用です。
>
> クローラロボット開発プラットフォーム CuGo V4 / クローラロボット開発プラットフォーム V3i をご利用の方は [cugo_ros2_motorcontroller2](https://github.com/CuboRex-Development/cugo_ros2_motorcontroller2) を参照してください。


# Table of Contents
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Architecture](#architecture)
- [Protocol](#protocol)
- [Note](#note)
- [License](#license)


# Features

cugo_v4.5_ros2_motorcontroller は、ROS 2パッケージ `cugo_v4.5_ros2_control` から受信した目標速度指令を ロボットに内臓された車両コントローラ CRST01A に転送し、現在の走行速度をROS側に返答するインターフェースモジュールです。


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

2. Arduino IDE で `cugo_v4.5_ros2_motorcontroller/cugo_v4.5_ros2_motorcontroller.ino` を開きます。

3. Arduino IDE のライブラリマネージャから `PacketSerial` をインストールします。

4. `crst01a_arduino_lib` を Arduino IDE のライブラリとして追加します。

5. ボードを Raspberry Pi Pico 2 / Pico 2W に設定し、スケッチを書き込みます。

6. CuGo V4.5 に搭載された基板のDIPスイッチを、RPiモードに設定します。

# Usage

1. CuGo V4.5 の Raspberry Pi Pico 2 WH と PC を USB ケーブルで接続します。

2. ROS 2パッケージ `cugo_v4.5_ros2_control` を起動すると、自動的にハンドシェイクが行われ通信が開始されます。

3. `/cmd_vel` トピックに速度指令を送ると、ロボットが動作します。

> [!TIP]
> 通信が正常に確立されない場合は、USB ケーブルを抜き差しして再接続してください。


# Architecture

```
┌────────────────────────────────────────────────────────────┐
│                        PC (ROS 2)                          │
│              cugo_v4.5_ros2_control ノード                  │
└─────────────────────────┬──────────────────────────────────┘
                          │ USB CDC (PacketSerial / COBS)
                          │ 115200 bps
┌─────────────────────────▼──────────────────────────────────┐
│           Raspberry Pi Pico 2 WH (インターフェースモジュール)  │
│            cugo_v4.5_ros2_motorcontroller                  │
└─────────────────────────┬──────────────────────────────────┘
                          │ UART0 (Serial1)
                          │ crst01a_arduino_lib
┌─────────────────────────▼──────────────────────────────────┐
│              CRST01A 車両コントローラ                         │
└────────────────────────────────────────────────────────────┘
```

**通信ポート**

| ポート | 用途 | 相手 |
|--------|------|------|
| Serial (USB CDC) | ROS 2コントローラノードとの通信 | PC |
| Serial1 (UART0) | CRST01A 車両コントローラとの通信 | CRST01A |

**主要ファイル**

| ファイル | 役割 |
|---------|------|
| `cugo_v4.5_ros2_motorcontroller.ino` | メインループ・フェイルセーフ・CRST01A状態監視 |
| `RosComm.h / .cpp` | ROSとのパケット解析・送信モジュール |
| `Crst01a.h / .cpp` | CRST01A通信ラッパー |


# Protocol

**プロダクトID**: `10000`（プロトコル識別子: `1`）

ロボット-ROS通信仕様 プロトコル識別子1 に準拠します。

**パケット構成**

```
[ヘッダ 8byte] + [ボディ 64byte] = 合計 72byte
```

ヘッダフォーマット:

| オフセット | サイズ | 内容 |
|-----------|--------|------|
| 0 | 2 byte | プロダクトID (uint16_t) |
| 2 | 2 byte | ロボットID (uint16_t) |
| 4 | 2 byte | 電文長 (uint16_t) |
| 6 | 2 byte | チェックサム (uint16_t, IPチェックサム, ボディのみ対象) |

受信ボディフォーマット:

| オフセット | サイズ | 内容 |
|-----------|--------|------|
| 0 | 2 byte | 目標X方向速度 (int16_t, 値×0.001 m/s) |
| 2 | 2 byte | 目標Y方向速度 (int16_t, 値×0.001 m/s) |
| 4 | 2 byte | 目標旋回速度 (int16_t, 値×0.001 rad/s) |
| 60 | 2 byte | プロダクトID (uint16_t, ヘッダと同値) |
| 62 | 2 byte | ロボットID (uint16_t, ヘッダと同値) |

送信ボディフォーマット:

| オフセット | サイズ | 内容 |
|-----------|--------|------|
| 0 | 2 byte | 現在X方向速度 (int16_t, 値×0.001 m/s) |
| 2 | 2 byte | 現在Y方向速度 (int16_t, 値×0.001 m/s) |
| 4 | 2 byte | 現在旋回速度 (int16_t, 値×0.001 rad/s) |
| 60 | 2 byte | プロダクトID (uint16_t) |
| 62 | 2 byte | ロボットID (uint16_t) |

**ハンドシェイク**

通信開始時、コントローラノードはイニシャル要求（ボディの先頭60バイトが全て0x00のパケット）を送信します。ロボット側はこれを受信するとCMDモードへ移行し、イニシャル応答を返します。

ハンドシェイクが完了するまで、速度指令は受け付けられません。

**フェイルセーフ**

100ms ごとに通信を監視し、5回連続（0.5秒間）パケットを受信しなかった場合は自動的に停止します。通信が回復した際はフェイルセーフ状態を解除し、再度ハンドシェイクを行います。


# Note

ご不明点がございましたら、[お問い合わせフォーム](https://cuborex.com/contact/)にてお問い合わせください。


# License

このプロジェクトはApache License 2.0のもと、公開されています。詳細はLICENSEをご覧ください。
