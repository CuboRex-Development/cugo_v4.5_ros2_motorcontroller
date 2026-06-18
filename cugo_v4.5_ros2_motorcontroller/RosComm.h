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
// enable_7_14 ビット定義 (body[6])
// セットされたビットに対応するフィールド (body[7-14]) のみ車両コントローラへ反映される
// ----------------------------------------------------------------------------
#define ENABLE_BIT_MODE_SWITCH      (0x01)  // body[7]:  モード切替
#define ENABLE_BIT_EMERGENCY_DECEL  (0x02)  // body[8]:  緊急減速
#define ENABLE_BIT_RESET_CTRL_ERROR (0x04)  // body[9]:  コントローラエラー解除
#define ENABLE_BIT_RESET_MD_ERROR   (0x08)  // body[10]: モータドライバエラー解除
#define ENABLE_BIT_HEADLIGHT        (0x10)  // body[11]: ヘッドライト制御
#define ENABLE_BIT_TOWERLIGHT       (0x20)  // body[12]: タワーライト制御
#define ENABLE_BIT_BUMPER_CONFIG    (0x40)  // body[13]: バンパー設定
#define ENABLE_BIT_BRAKE_CONFIG     (0x80)  // body[14]: ブレーキ設定

// ----------------------------------------------------------------------------
// 受信データ構造体 (PC→RPi)
// ----------------------------------------------------------------------------
typedef struct {
	bool     isInitialReq;               // イニシャル要求かどうか
	int16_t  xSpeed;                     // 目標X方向速度 (値×0.001m/s)
	int16_t  ySpeed;                     // 目標Y方向速度 (値×0.001m/s)
	int16_t  yawSpeed;                   // 目標旋回速度  (値×0.001rad/s)
	uint8_t  enable_7_14;                // body[6]: 有効フィールドビットマスク
	uint8_t  mode_switch;                // body[7]: 0x80=RCモード / 0x81=CMDモード
	uint8_t  emergency_decel;            // body[8]: 緊急減速トリガ (非零で発動)
	uint8_t  reset_controller_error;     // body[9]: 解除するコントローラエラービット
	uint8_t  reset_motordriver_error;    // body[10]: 解除するモータドライバエラービット
	uint8_t  headlight_control;          // body[11]: ヘッドライト制御値
	uint8_t  towerlight_control;         // body[12]: タワーライト制御値
	uint8_t  bumper_config;              // body[13]: バンパー設定値
	uint8_t  brake_config;               // body[14]: ブレーキ設定値
} RosCommRecvData;

// ----------------------------------------------------------------------------
// 送信データ構造体 (RPi→PC)
// ----------------------------------------------------------------------------
typedef struct {
	int16_t  curX;                        // 現在X方向速度 (値×0.001m/s)
	int16_t  curY;                        // 現在Y方向速度 (値×0.001m/s)
	int16_t  curYaw;                      // 現在旋回速度  (値×0.001rad/s)
	uint8_t  controller_status;           // body[6]:  コントローラステータス
	uint8_t  controller_error;            // body[7]:  コントローラエラービット
	uint8_t  motordriver_error;           // body[8]:  モータドライバエラービット
	uint16_t driver_voltage_raw;          // body[9-10]: ドライバ電圧 (raw × 0.1 V)
	uint8_t  headlight_status;            // body[11]: ヘッドライト状態
	uint8_t  towerlight_status;           // body[12]: タワーライト状態
	uint8_t  io_input_status;             // body[13]: 外部4bitデジタル入力
	uint32_t encoder_motor[4];            // body[14-29]: エンコーダカウント
	uint16_t motordriver_temp[4];         // body[30-37]: モータドライバ温度 [℃]
	uint16_t motordriver_error_code[4];   // body[38-45]: モータドライバエラーコード
	uint8_t  bumper_config;               // body[46]: バンパー設定値
	uint8_t  brake_config;                // body[47]: ブレーキ設定値
	uint8_t  crst01a_data_read_error;     // body[59]: bit0 = データ読み取りエラーあり
} RosCommSendData;

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
//       data        : 送信データ構造体
//       isInitialReq: イニシャル応答かどうか (trueの場合、速度・IOフィールドは全て0)
// ----------------------------------------------------------------------------
void RosCommSendResponse(PacketSerial *ps, const RosCommSendData *data, bool isInitialReq);

#endif // ROS_COMM_H_
