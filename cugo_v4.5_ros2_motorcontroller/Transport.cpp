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
    Serial.setBlocking(false);
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
    Serial.setBlocking(false);
    delay(1000);
#endif

    // UART1 ピン設定・初期化 (GP8: TX, GP9: RX)
    Serial2.setTX(8);
    Serial2.setRX(9);
    Serial2.begin(BOX_CN_BAUD_RATE);

    // PacketSerial 初期化 (COBSエンコード/デコード、UART1/Serial2 使用)
    // PacketSerial.begin() は内部で Serial (USB CDC) を初期化するが、
    // 直後に setStream(&Serial2) で Serial2 に切り替えるため問題ない
    _packetSerial.begin(BOX_CN_BAUD_RATE);
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
// Bluetooth SPP モード (USE_BLUETOOTH 定義時)
// ============================================================
#elif defined(USE_BLUETOOTH)

// 接続確立後にリンク監視タイムアウト無効化コマンドを遅延送信するための変数。
// hci_send_cmd() はコールバック内から直接呼ぶと BTstack の再入エラーになるため、
// ハンドルをここに保存し update() から安全なタイミングで送信する。
static uint16_t _pendingSupervisionTimeoutHandle = HCI_CON_HANDLE_INVALID;

// HCI イベントハンドラ: 接続管理 (常時有効) + ペアリング/接続/切断ログ (INFO_SERIAL 時のみ)
//
// パケット構造 (HCI_EVENT_USER_CONFIRMATION_REQUEST / HCI_EVENT_USER_PASSKEY_NOTIFICATION):
//   Byte 0   : Event code  Bytes 2-7: BD_ADDR  Bytes 8-11: Passkey (uint32, little-endian)
// パケット構造 (HCI_EVENT_SIMPLE_PAIRING_COMPLETE):
//   Byte 0: Event code  Byte 2: Status  Bytes 3-8: BD_ADDR
// パケット構造 (HCI_EVENT_CONNECTION_COMPLETE):
//   Byte 0: Event code  Byte 2: Status  Bytes 3-4: Handle  Bytes 5-10: BD_ADDR  Byte 11: Link type
// パケット構造 (HCI_EVENT_DISCONNECTION_COMPLETE):
//   Byte 0: Event code  Byte 2: Status  Bytes 3-4: Handle  Byte 5: Reason
static void _btEventHandler(uint8_t packet_type, uint16_t channel,
                             uint8_t* packet, uint16_t size) {
    (void)channel; (void)size;
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (packet[0]) {
#ifdef INFO_SERIAL
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        case HCI_EVENT_USER_PASSKEY_NOTIFICATION: {
            uint32_t passkey = little_endian_read_32(packet, 8);
            Serial.println("ペアリング要求を受けました。接続する機器に表示されたPINが一致しているか確認してください。");
            Serial.printf("ペアリングPIN: %06u\n", (unsigned int)passkey);
            break;
        }
        case HCI_EVENT_SIMPLE_PAIRING_COMPLETE:
            if (packet[2] == 0x00) {
                Serial.println("ペアリング完了");
            } else {
                Serial.printf("ペアリング失敗 (status=0x%02X)\n", packet[2]);
            }
            break;
#endif  // INFO_SERIAL
        case HCI_EVENT_CONNECTION_COMPLETE:
            // ACL接続 (Link type=0x01) のみ対象。SCO等は無視する
            if (packet[2] == 0x00 && packet[11] == 0x01) {
                // コールバック内から hci_send_cmd() を直接呼ぶと BTstack の再入エラーになるため、
                // ハンドルを保存して update() から送信する
                _pendingSupervisionTimeoutHandle = little_endian_read_16(packet, 3);
#ifdef INFO_SERIAL
                // HCI パケット内の BD_ADDR は little-endian のため逆順で表示する
                Serial.printf("ペアリング済みの機器と接続しました (%02X:%02X:%02X:%02X:%02X:%02X)\n",
                              packet[10], packet[9], packet[8],
                              packet[7],  packet[6], packet[5]);
#endif
            }
            break;
#ifdef INFO_SERIAL
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (packet[2] == 0x00) {
                // reason コードで切断原因を特定できる
                // 0x08: Connection Timeout (監視タイムアウト)
                // 0x13: Remote User Terminated (PC側が切断)
                // 0x16: Connection Terminated by Local Host (Pico側が切断)
                Serial.printf("切断しました (reason=0x%02X)\n", packet[5]);
            }
            break;
#endif  // INFO_SERIAL
        default:
            break;
    }
}
// BTstack イベントハンドラ登録用構造体 (begin() の外側でスコープを持たせる)
static btstack_packet_callback_registration_t _btEventCallbackReg;

void Transport::begin(PacketHandlerFn handler) {
    // USBシリアル出力が必要なフラグがいずれか有効なら Serial を起動する
#if defined(INFO_SERIAL) || defined(DEBUG_BT_TX_LOG) || defined(DEBUG_BT_RX_LOG) || \
    defined(DEBUG_BT_TX_RAW_LOG) || defined(DEBUG_BT_RX_RAW_LOG)
    Serial.begin(115200);
    Serial.setBlocking(false);
    delay(1000);
#endif

    SerialBT.begin(115200);

    // SerialBT.begin() は BTstack の初期化を非同期で開始する。
    // HCI コントローラが BD アドレスを返すまでポーリングして完了を待つ。
    // 初期化完了前は gap_local_bd_addr() が 00:00:00:00:00:00 を返す。
#ifdef INFO_SERIAL
    Serial.println("Mode: Bluetoothモード");
    Serial.print("コントローラ起動中");
#endif
    bd_addr_t addr;
    const uint32_t kBdAddrTimeoutMs = 3000;
    uint32_t deadline = millis() + kBdAddrTimeoutMs;
    do {
        gap_local_bd_addr(addr);
        if (addr[0] || addr[1] || addr[2] || addr[3] || addr[4] || addr[5]) break;
        delay(10);
#ifdef INFO_SERIAL
        Serial.print(".");
#endif
    } while (millis() < deadline);

    // SerialBT.begin() 内部でデフォルト名("PicoW Serial XX:XX:...")が設定されるため、
    // BTstack 初期化完了後に BT_DEVICE_NAME で上書きする
    gap_set_local_name(BT_DEVICE_NAME);

    // HCI イベントハンドラを登録する (INFO_SERIAL の有無によらず常時登録)
    // 接続管理 (リンク監視タイムアウト無効化) は INFO_SERIAL 無効時も動作させる必要があるため
    _btEventCallbackReg.callback = &_btEventHandler;
    hci_add_event_handler(&_btEventCallbackReg);

#ifdef INFO_SERIAL
    Serial.println(" 完了");
    Serial.print("Device Name   : ");
    Serial.println(BT_DEVICE_NAME);
    Serial.printf("BT MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    Serial.println("ペアリング待機中...");
#endif

    // PacketSerial 初期化 (COBSエンコード/デコード、SerialBT 使用)
    // PacketSerial.begin() は内部で Serial (USB CDC) を初期化するが、
    // 直後に setStream(&SerialBT) で SerialBT に切り替えるため問題ない
    _packetSerial.begin(115200);
#if defined(DEBUG_BT_TX_RAW_LOG) || defined(DEBUG_BT_RX_RAW_LOG)
    _debugStream.setInner(&SerialBT);
    _packetSerial.setStream(&_debugStream);
#else
    _packetSerial.setStream(&SerialBT);
#endif
    _packetSerial.setPacketHandler(handler);
}

void Transport::update(void (*onNewClient)()) {
    (void)onNewClient;  // Bluetoothモードでは使用しない

    // BTstack のペンディングイベントを先に処理してから送信可否を確認する
    _packetSerial.update();

    // 接続確立時に保存した接続ハンドルに対してリンク監視タイムアウトを無効化する。
    // hci_send_cmd() はコールバック内から呼ぶと再入エラーになるため、
    // ここで BTstack がコマンド送信可能なタイミングを確認してから送信する。
    if (_pendingSupervisionTimeoutHandle != HCI_CON_HANDLE_INVALID &&
        hci_can_send_command_packet_now()) {
        hci_send_cmd(&hci_write_link_supervision_timeout,
                     _pendingSupervisionTimeoutHandle, 0x0000);
        _pendingSupervisionTimeoutHandle = HCI_CON_HANDLE_INVALID;
    }

    if (_packetSerial.overflow()) {
        // 現状は無視 (必要に応じてエラー通知等を追加)
    }
}

// ============================================================
// Serial モード (USE_WIFI・USE_BOX_CN・USE_BLUETOOTH いずれも未定義)
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
