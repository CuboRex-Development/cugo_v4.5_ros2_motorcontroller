// ROS通信モジュール
//
// ロボット-ROS通信仕様 プロトコル識別子1 (プロダクトID 10000) の
// パケット解析・検証・応答生成を担当する
//
// 依存: PacketSerial
#ifndef ROS_COMM_H_
#define ROS_COMM_H_

#include "Arduino.h"
#include <PacketSerial.h>

// ----------------------------------------------------------------------------
// プロトコル定数
// ----------------------------------------------------------------------------
#define PRODUCT_ID      (10000)     // CuGo V4.5 プロダクトID
#define ROBOT_ID        (0)         // ロボットID (個体識別不要のため0)
#define PROTOCOL_ID     ((PRODUCT_ID) / 10000)  // プロトコル識別子 = 1

// ヘッダサイズ・ボディサイズ
#define SERIAL_HEADER_SIZE      (8)
#define SERIAL_BIN_BUFF_SIZE    (64)

// イニシャル要求判定範囲: body[0-59]が全て0かチェックする
#define INITIAL_REQ_CHECK_LEN   (60)

// ----------------------------------------------------------------------------
// 受信データ構造体
// ----------------------------------------------------------------------------
typedef struct {
	bool isInitialReq;  // イニシャル要求かどうか
	int16_t xSpeed;     // 目標X方向速度 (値×0.001m/s)
	int16_t ySpeed;     // 目標Y方向速度 (値×0.001m/s)
	int16_t yawSpeed;   // 目標旋回速度  (値×0.001rad/s)
} RosCommRecvData;

// ----------------------------------------------------------------------------
// 受信パケット解析・検証
// プロトコル識別子・チェックサムを検証し、ボディを解析する
// 引数: buffer        : 受信バッファ (PacketSerialコールバックの引数をそのまま渡す)
//       size          : バッファサイズ
//       outData       : 解析結果格納先
//       handshakeDone : ハンドシェイク済みかどうか (デフォルト: false)
//                       trueの場合、イニシャル要求判定をスキップし、
//                       常に速度指令として解析する
// 戻り値: 検証OKならtrue、プロトコルエラー・チェックサム不一致はfalse
// ----------------------------------------------------------------------------
bool RosCommParsePacket(const uint8_t *buffer, size_t size, RosCommRecvData *outData, bool handshakeDone = false);

// ----------------------------------------------------------------------------
// 応答パケット送信
// 引数: ps          : PacketSerialインスタンス
//       curX        : 現在X方向速度 (値×0.001m/s)
//       curY        : 現在Y方向速度 (値×0.001m/s)
//       curYaw      : 現在旋回速度  (値×0.001rad/s)
//       isInitialReq: イニシャル応答かどうか (trueの場合、速度フィールドは全て0)
// ----------------------------------------------------------------------------
void RosCommSendResponse(PacketSerial *ps, int16_t curX, int16_t curY, int16_t curYaw, bool isInitialReq);

#endif // ROS_COMM_H_
