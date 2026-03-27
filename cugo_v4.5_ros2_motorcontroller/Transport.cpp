// トランスポート抽象化モジュール 実装
//
// ユーザーがこのファイルを編集する必要はありません。

#include "Transport.h"

// ============================================================
// WiFi モード
// ============================================================
#ifdef USE_WIFI

void Transport::_initWifi() {
    // USBシリアル出力が必要なフラグがいずれか有効なら Serial を起動する
#if defined(INFO_SERIAL) || defined(DEBUG_WIFI_TX_LOG) || defined(DEBUG_WIFI_RX_LOG) || \
    defined(DEBUG_WIFI_TX_RAW_LOG) || defined(DEBUG_WIFI_RX_RAW_LOG) || defined(DEBUG_SERIAL_STATS)
    Serial.begin(115200);
    delay(1000);
#endif

#ifdef WIFI_AP_MODE
    // ----------------------------------------------------------
    // APモード: Pico 2WH 自身がアクセスポイントとして起動する
    // ----------------------------------------------------------
#ifdef INFO_SERIAL
    Serial.println("Mode: WiFi APモード");
#endif

    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);

#ifdef INFO_SERIAL
    Serial.print("AP started. IP: ");
    Serial.println(WiFi.softAPIP());
#endif

#else
    // ----------------------------------------------------------
    // Stationモード: 既存の WiFi ルータに接続する
    // ----------------------------------------------------------
#ifdef INFO_SERIAL
    Serial.println("Mode: WiFi Stationモード");
    Serial.print("Connecting to WiFi");
#endif

#ifdef WIFI_STATIC_IP
    WiFi.config(WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET);
#endif

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
#ifdef INFO_SERIAL
        Serial.print(".");
#endif
        delay(100);
    }

#ifdef INFO_SERIAL
    Serial.println();
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
#endif

#endif // WIFI_AP_MODE
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
        _client.stop();  // 半開き接続のリソースを解放する
        WiFiClient newClient = _server.accept();
        if (newClient) {
            newClient.setNoDelay(true);  // Nagle's Algorithm を無効化し応答遅延を防ぐ
            _client = newClient;
            _debugStream.setInner(&_client);
            _packetSerial.setStream(&_debugStream);
            if (onNewClient != nullptr) {
                onNewClient();
            }
        }
    }
    // クライアント接続中のみ PacketSerial 受信処理を実行する
    if (_client.connected()) {
        _packetSerial.update();
        if (_packetSerial.overflow()) {
            _client.stop();  // バッファ詰まりを解消するため切断してリセット
        }
    }
}

// ============================================================
// BOX_CN モード (USE_BOX_CN 定義時: 基板上のボックスコネクタにあるUARTピンを使用するモード)
// ============================================================
#elif defined(USE_BOX_CN)

void Transport::begin(PacketHandlerFn handler) {
    // USBシリアル出力が必要なフラグがいずれか有効なら Serial を起動する
#if defined(DEBUG_BOX_CN_TX_LOG) || defined(DEBUG_BOX_CN_RX_LOG) || \
    defined(DEBUG_BOX_CN_TX_RAW_LOG) || defined(DEBUG_BOX_CN_RX_RAW_LOG)
    Serial.begin(115200);
    delay(1000);
#endif

    // UART1 ピン設定・初期化 (GP8: TX, GP9: RX)
    Serial2.setTX(8);
    Serial2.setRX(9);
    Serial2.begin(115200);

    // PacketSerial 初期化 (COBSエンコード/デコード、UART1/Serial2 使用)
    // PacketSerial.begin() は内部で Serial (USB CDC) を初期化するが、
    // 直後に setStream(&Serial2) で Serial2 に切り替えるため問題ない
    _packetSerial.begin(115200);
#if defined(DEBUG_BOX_CN_TX_RAW_LOG) || defined(DEBUG_BOX_CN_RX_RAW_LOG)
    _debugStream.setInner(&Serial2);
    _packetSerial.setStream(&_debugStream);
#else
    _packetSerial.setStream(&Serial2);
#endif
    _packetSerial.setPacketHandler(handler);

    // 起動直後のシリアルバッファをクリア
    delay(100);
    while (Serial2.available() > 0) {
        Serial2.read();
    }
}

void Transport::update(void (*onNewClient)()) {
    (void)onNewClient;  // BOX_CN モードでは使用しない

    _packetSerial.update();

    if (_packetSerial.overflow()) {
        // 現状は無視 (必要に応じてエラー通知等を追加)
    }
}

// ============================================================
// Serial モード (USE_WIFI・USE_BOX_CN いずれも未定義)
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

#endif // USE_WIFI / USE_BOX_CN
