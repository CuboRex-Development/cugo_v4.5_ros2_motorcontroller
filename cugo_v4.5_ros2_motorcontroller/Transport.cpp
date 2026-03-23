// トランスポート抽象化モジュール 実装
//
// ユーザーがこのファイルを編集する必要はありません。

#include "Transport.h"

// ============================================================
// WiFi モード
// ============================================================
#ifdef USE_WIFI

void Transport::_initWifi() {
#ifdef WIFI_DEBUG_SERIAL
    Serial.begin(115200);
    Serial.print("Connecting to WiFi");
#endif

#ifdef WIFI_STATIC_IP
    WiFi.config(WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET);
#endif

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
#ifdef WIFI_DEBUG_SERIAL
        Serial.print(".");
#endif
        delay(100);
    }

#ifdef WIFI_DEBUG_SERIAL
    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
#endif
}

void Transport::begin(PacketHandlerFn handler) {
    _initWifi();
    _server.begin();
    // WiFi モードでは setStream はクライアント接続後に行うため、ここではハンドラ登録のみ
    _packetSerial.setPacketHandler(handler);
}

void Transport::update(void (*onNewClient)()) {
    // クライアント未接続または切断時: 新規接続を受け入れる
    if (!_client.connected()) {
        WiFiClient newClient = _server.accept();
        if (newClient) {
            _client = newClient;
            _packetSerial.setStream(&_client);
            if (onNewClient != nullptr) {
                onNewClient();
            }
        }
    }
    // クライアント接続中のみ PacketSerial 受信処理を実行する
    if (_client.connected()) {
        _packetSerial.update();
        if (_packetSerial.overflow()) {
            // 現状は無視 (必要に応じてエラー通知等を追加)
        }
    }
}

// ============================================================
// Serial モード (USE_WIFI 未定義)
// ============================================================
#else

void Transport::begin(PacketHandlerFn handler) {
    // PacketSerial 初期化 (COBSエンコード/デコード、USB CDC Serial 使用)
    _packetSerial.begin(115200);
    _packetSerial.setStream(&Serial);
    _packetSerial.setPacketHandler(handler);

    // 起動直後のシリアルバッファをクリア
    delay(100);
    while (Serial.available() > 0) {
        Serial.read();
    }
}

void Transport::update(void (*onNewClient)()) {
    (void)onNewClient;  // Serial モードでは使用しない

    _packetSerial.update();

    if (_packetSerial.overflow()) {
        // 現状は無視 (必要に応じてエラー通知等を追加)
    }
}

#endif // USE_WIFI
