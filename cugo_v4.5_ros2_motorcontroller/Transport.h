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

#ifdef USE_WIFI
// WiFi TCP ストリームの生バイトをUSBシリアルにログ出力するデバッグ用ストリームラッパー。
// DEBUG_WIFI_TX_RAW_LOG / DEBUG_WIFI_RX_RAW_LOG が有効な場合のみパケットバッファを使用します。
// COBS パケット終端の 0x00 を区切りとして1パケット分をまとめて出力します。
class DebugStream : public Stream {
public:
    // ラップ対象ストリームを設定します。新規クライアント接続時に呼び出します。
    void setInner(WiFiClient* s) {
        _inner = s;
        _readLen  = 0;  // 接続切り替え時に古いデータを残さないためバッファをリセット
        _readHead = 0;
#ifdef DEBUG_WIFI_RX_RAW_LOG
        _rxLen = 0;
#endif
#ifdef DEBUG_WIFI_TX_RAW_LOG
        _txLen = 0;
#endif
    }

    // 内部バッファに残りバイトがあればその量を、なければ TCP の available() を返す
    int  available() override {
        if (_readLen > 0) return (int)_readLen;
        return _inner ? _inner->available() : 0;
    }
    // 内部バッファに残りバイトがあればその先頭を、なければ TCP の peek() を返す
    int  peek() override {
        if (_readLen > 0) return (int)_readBuf[_readHead];
        return _inner ? _inner->peek() : -1;
    }
    void flush() override { if (_inner) _inner->flush(); }

    int read() override {
        if (!_inner) return -1;
        // 内部バッファが空のとき、TCP から available() バイトを一括読み出しする
        // これにより WiFiClient.read() の1バイト毎のオーバーヘッドを削減する
        if (_readLen == 0) {
            int avail = _inner->available();
            if (avail <= 0) return -1;
            int n = _inner->read(_readBuf, min((size_t)avail, kReadBufSize));
            if (n <= 0) return -1;
            _readLen  = (size_t)n;
            _readHead = 0;
        }
        // 内部バッファから 1 バイト返す
        uint8_t b = _readBuf[_readHead++];
        _readLen--;
#ifdef DEBUG_WIFI_RX_RAW_LOG
        if (b == 0x00) {
            Serial.print("[WiFi RX RAW] ");
            for (size_t i = 0; i < _rxLen; i++) {
                if (_rxBuf[i] < 0x10) Serial.print('0');
                Serial.print(_rxBuf[i], HEX);
                Serial.print(' ');
            }
            Serial.println();
            _rxLen = 0;
        } else if (_rxLen < kLogBufSize) {
            _rxBuf[_rxLen++] = b;
        }
#endif
        return (int)b;
    }

    size_t write(uint8_t b) override {
#ifdef DEBUG_WIFI_TX_RAW_LOG
        if (b == 0x00) {
            Serial.print("[WiFi TX RAW] ");
            for (size_t i = 0; i < _txLen; i++) {
                if (_txBuf[i] < 0x10) Serial.print('0');
                Serial.print(_txBuf[i], HEX);
                Serial.print(' ');
            }
            Serial.println();
            _txLen = 0;
        } else if (_txLen < kLogBufSize) {
            _txBuf[_txLen++] = b;
        }
#endif
        return _inner ? _inner->write(b) : 1;
    }

    size_t write(const uint8_t* buf, size_t size) override {
#ifdef DEBUG_WIFI_TX_RAW_LOG
        for (size_t i = 0; i < size; i++) {
            if (buf[i] == 0x00) {
                Serial.print("[WiFi TX RAW] ");
                for (size_t j = 0; j < _txLen; j++) {
                    if (_txBuf[j] < 0x10) Serial.print('0');
                    Serial.print(_txBuf[j], HEX);
                    Serial.print(' ');
                }
                Serial.println();
                _txLen = 0;
            } else if (_txLen < kLogBufSize) {
                _txBuf[_txLen++] = buf[i];
            }
        }
#endif
        return _inner ? _inner->write(buf, size) : size;
    }

private:
    WiFiClient* _inner = nullptr;

    // 一括読み出しバッファ: TCP から available() バイトをまとめて取得し PacketSerial に提供する
    // サイズは観測された最大蓄積量 444B (6パケット) を上回る余裕を持たせた値
    // available() がこのサイズを超える場合は複数回に分けて読み出す (動作は正しく継続する)
    static const size_t kReadBufSize = 512;
    uint8_t _readBuf[kReadBufSize];
    size_t  _readHead = 0;
    size_t  _readLen  = 0;

    // RAWログ用バッファ: COBS パケット終端 (0x00) までのバイト列を蓄積してまとめて出力する
    static const size_t kLogBufSize = 128;  // 1パケット最大長 (74バイト + マージン)
#ifdef DEBUG_WIFI_RX_RAW_LOG
    uint8_t _rxBuf[kLogBufSize];
    size_t  _rxLen = 0;
#endif
#ifdef DEBUG_WIFI_TX_RAW_LOG
    uint8_t _txBuf[kLogBufSize];
    size_t  _txLen = 0;
#endif
};
#endif // USE_WIFI

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

    // TCPクライアントの受信バッファに溜まっているバイト数を返します (WiFiモードのみ有効)
    // DEBUG_SERIAL_STATS でのパケット蓄積量の計測に使用します
    int clientAvailable() {
#ifdef USE_WIFI
        return _client.available();
#else
        return 0;
#endif
    }

private:
    PacketSerial _packetSerial;

#ifdef USE_WIFI
    WiFiServer   _server{WIFI_TCP_PORT};
    WiFiClient   _client;
    DebugStream  _debugStream;  // RAWログ用ストリームラッパー (_client をラップ)
    void _initWifi();
#endif
};

#endif // TRANSPORT_H_
