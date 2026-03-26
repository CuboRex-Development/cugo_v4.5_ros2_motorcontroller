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
// 
//   USB-Serial モード (デフォルト)
//     → USE_WIFI をコメントアウトのまま
//
//   WiFi APモード (Pico 2WH 自身がアクセスポイント・ルータ不要)
//     → USE_WIFI と WIFI_AP_MODE の両方のコメントを外す
//
//   WiFi Stationモード (外部ルータ経由)
//     → USE_WIFI のコメントを外す
//       WIFI_AP_MODE はコメントアウトのまま

// #define USE_WIFI
// #define WIFI_AP_MODE    // USE_WIFI 定義時のみ有効

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

// 静的IPを使用する場合はコメントを外して各アドレスを設定してください
// #define WIFI_STATIC_IP
#ifdef WIFI_STATIC_IP
#define WIFI_IP         IPAddress(192, 168,  0, 101)
#define WIFI_GATEWAY    IPAddress(192, 168,  0,   1)
#define WIFI_SUBNET     IPAddress(255, 255, 255,  0)
#endif // WIFI_STATIC_IP

#endif // USE_WIFI / WIFI_AP_MODE

#endif // CONFIG_H_
