// ユーザー設定ファイル
//
// 通信モードの選択と各モードのパラメータを設定します。
// 編集が必要なのは このファイルのみ です。
//
// 通信モード選択:
//   USB-Serial モード (デフォルト): 以下の USE_WIFI 行はコメントアウトのまま
//   WiFi モード:                   以下の USE_WIFI 行のコメントを外してください

#ifndef CONFIG_H_
#define CONFIG_H_

// #define USE_WIFI

// ============================================================
// WiFi モード設定 (USE_WIFI 定義時のみ有効)
// ============================================================
#ifdef USE_WIFI

// 接続先 WiFi ネットワーク設定
#define WIFI_SSID       "your_ssid"
#define WIFI_PASSWORD   "your_password"
#define WIFI_TCP_PORT   (8080)

// 接続状況と自身のIPをUSBシリアルに出力する場合は以下のコメントを外してください
// #define WIFI_DEBUG_SERIAL

// 静的IPを使用する場合は以下のコメントを外して各アドレスを設定してください
// #define WIFI_STATIC_IP
#ifdef WIFI_STATIC_IP
#define WIFI_IP         IPAddress(192, 168,  0, 101)
#define WIFI_GATEWAY    IPAddress(192, 168,  0,   1)
#define WIFI_SUBNET     IPAddress(255, 255, 255,  0)
#endif // WIFI_STATIC_IP

#endif // USE_WIFI

#endif // CONFIG_H_
