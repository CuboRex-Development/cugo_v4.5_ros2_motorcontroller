// トランスポート抽象化モジュール
//
// USB-Serial と WiFi TCP を同一インターフェースで提供します。
// 使用するトランスポートは config.h の USE_WIFI で切り替えます。
//
// ユーザーがこのファイルを編集する必要はありません。

#ifndef TRANSPORT_H_
#define TRANSPORT_H_

#include "config.h"
#include <PacketSerial.h>
#ifdef USE_WIFI
#include <WiFi.h>
#endif

// PacketSerial パケット受信コールバック型
typedef void (*PacketHandlerFn)(const uint8_t*, size_t);

// トランスポート管理クラス
// USB-Serial または WiFi TCP の初期化・受信処理をカプセル化します。
class Transport {
public:
    // トランスポートを初期化します。setup() から呼び出します。
    // handler: パケット受信時に呼び出されるコールバック関数
    void begin(PacketHandlerFn handler);

    // 受信処理を実行します。loop() から毎ループ呼び出します。
    // onNewClient: WiFi モードで新規クライアント接続時に呼ばれるコールバック
    //              (Serial モードでは呼び出されません、nullptr 可)
    void update(void (*onNewClient)() = nullptr);

    // 応答送信に使用する PacketSerial への参照を返します
    PacketSerial& serial() { return _packetSerial; }

private:
    PacketSerial _packetSerial;

#ifdef USE_WIFI
    WiFiServer _server{WIFI_TCP_PORT};
    WiFiClient _client;
    void _initWifi();
#endif
};

#endif // TRANSPORT_H_
