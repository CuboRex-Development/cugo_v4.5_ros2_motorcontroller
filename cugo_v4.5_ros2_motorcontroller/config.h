// ユーザー設定ファイル
//
// 通信モードの選択と各モードのパラメータを設定します。
// 編集が必要なのは このファイルのみ です。

#ifndef CONFIG_H_
#define CONFIG_H_

// ============================================================
// 通信モード選択
// ============================================================
// このセクションで定義されている #define のコメントを外すことで、使用する通信モードを選択してください。
// 複数を同時に有効にすることはできません。
//
//   USB-Serial モード (デフォルト)
//     → USE_WIFI・USE_BOX_CN をコメントアウトのまま
//
//   BOX_CN モード (基板上のボックスコネクタにあるUARTピンを使用するモード)
//     → USE_BOX_CN のコメントを外す
//       USE_WIFI はコメントアウトのまま
//
//   WiFi APモード (Pico 2WH 自身がアクセスポイント・ルータ不要)
//     → USE_WIFI と WIFI_AP_MODE の両方のコメントを外す
//       USE_BOX_CN はコメントアウトのまま
//
//   WiFi Stationモード (外部ルータ経由)
//     → USE_WIFI のコメントを外す
//       WIFI_AP_MODE・USE_BOX_CN はコメントアウトのまま

// #define USE_BOX_CN      // 基板上のボックスコネクタにあるUARTピンを使用するモード
// #define USE_WIFI
// #define WIFI_AP_MODE    // USE_WIFI 定義時のみ有効

#if defined(USE_BOX_CN) && defined(USE_WIFI)
#error "USE_BOX_CN と USE_WIFI を同時に定義することはできません"
#endif

// ============================================================
// 各モードのパラメータ設定
// ============================================================

// ------------------------------------------------------------
// WiFi APモード設定 (USE_WIFI かつ WIFI_AP_MODE 定義時のみ有効)
// ------------------------------------------------------------
#if defined(USE_WIFI) && defined(WIFI_AP_MODE)

#define WIFI_AP_SSID        "CuGo_AP"   // アクセスポイントの SSID
#define WIFI_AP_PASSWORD    "cugo1234"  // アクセスポイントのパスワード
#define WIFI_TCP_PORT       (8080)      // TCPポート番号
#define WIFI_AP_CHANNEL     (1)         // アクセスポイントのチャンネル (1, 6, 11 を推奨)

// 起動時に通信モードとIPアドレスをUSBシリアルに出力します (無効にする場合はコメントアウト)
#define INFO_SERIAL

// --- デバッグログ (開発者向け、通常はコメントアウト) ---
// #define DEBUG_WIFI_TX_LOG       // COBSエンコード前の送信データをUSBシリアルに出力
// #define DEBUG_WIFI_RX_LOG       // COBSデコード後の受信データをUSBシリアルに出力
// #define DEBUG_WIFI_TX_RAW_LOG   // COBSエンコード後の送信データをUSBシリアルに出力
// #define DEBUG_WIFI_RX_RAW_LOG   // COBSデコード前の受信データをUSBシリアルに出力
// #define DEBUG_SERIAL_STATS      // 1秒ごとにループ統計をUSBシリアルに出力
//                                 //   loops/s   : ループ実行回数 (低い = ループが詰まっている)
//                                 //   maxLoop   : ループ1回の最大所要時間 [µs]
//                                 //   maxUpdate : transport.update() の最大所要時間 [µs] (WiFi TCP受信処理)
//                                 //   maxDelay1 : delay(1) の実際の最大所要時間 [µs] (CYW43スタック処理)

// ------------------------------------------------------------
// WiFi Stationモード設定 (USE_WIFI 定義かつ WIFI_AP_MODE 未定義時のみ有効)
// ------------------------------------------------------------
#elif defined(USE_WIFI)

#define WIFI_SSID           "your_ssid"     // 接続先 WiFi の SSID
#define WIFI_PASSWORD       "your_password" // 接続先 WiFi のパスワード
#define WIFI_TCP_PORT       (8080)          // TCPポート番号

// 起動時に通信モードとIPアドレスをUSBシリアルに出力します (無効にする場合はコメントアウト)
#define INFO_SERIAL

// --- デバッグログ (開発者向け、通常はコメントアウト) ---
// #define DEBUG_WIFI_TX_LOG       // COBSエンコード前の送信データをUSBシリアルに出力
// #define DEBUG_WIFI_RX_LOG       // COBSデコード後の受信データをUSBシリアルに出力
// #define DEBUG_WIFI_TX_RAW_LOG   // COBSエンコード後の送信データをUSBシリアルに出力
// #define DEBUG_WIFI_RX_RAW_LOG   // COBSデコード前の受信データをUSBシリアルに出力
// #define DEBUG_SERIAL_STATS      // 1秒ごとにループ統計をUSBシリアルに出力
//                                 //   loops/s   : ループ実行回数 (低い = ループが詰まっている)
//                                 //   maxLoop   : ループ1回の最大所要時間 [µs]
//                                 //   maxUpdate : transport.update() の最大所要時間 [µs] (WiFi TCP受信処理)
//                                 //   maxDelay1 : delay(1) の実際の最大所要時間 [µs] (CYW43スタック処理)

// 静的IPを使用する場合はコメントを外して各アドレスを設定してください
// #define WIFI_STATIC_IP
#ifdef WIFI_STATIC_IP
#define WIFI_IP         IPAddress(192, 168,  0, 101)
#define WIFI_GATEWAY    IPAddress(192, 168,  0,   1)
#define WIFI_SUBNET     IPAddress(255, 255, 255,  0)
#endif // WIFI_STATIC_IP

// ------------------------------------------------------------
// BOX_CN モード設定 (USE_BOX_CN 定義時のみ有効)
// ------------------------------------------------------------
#elif defined(USE_BOX_CN)

// --- デバッグログ (開発者向け、通常はコメントアウト) ---
// #define DEBUG_BOX_CN_TX_LOG       // COBSエンコード前の送信データをUSBシリアルに出力
// #define DEBUG_BOX_CN_RX_LOG       // COBSデコード後の受信データをUSBシリアルに出力
// #define DEBUG_BOX_CN_TX_RAW_LOG   // COBSエンコード後の送信データをUSBシリアルに出力
// #define DEBUG_BOX_CN_RX_RAW_LOG   // COBSデコード前の受信データをUSBシリアルに出力

#endif // USE_WIFI / WIFI_AP_MODE / USE_BOX_CN

#endif // CONFIG_H_
