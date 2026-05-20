// ROS通信モジュール 実装
#include "RosComm.h"
#include "config.h"


// ----------------------------------------------------------------------------
// ヘッダ内オフセット
// ----------------------------------------------------------------------------
#define RECV_HEADER_PRODUCT_ID_PTR  (0)     // プロダクトID
#define RECV_HEADER_ROBOT_ID_PTR    (2)     // ロボットID
#define RECV_HEADER_LENGTH_PTR      (4)     // 電文長
#define RECV_HEADER_CHECKSUM_PTR    (6)     // チェックサム

// ----------------------------------------------------------------------------
// 受信ボディ内オフセット (PC→RPi)
// ----------------------------------------------------------------------------
#define TARGET_X_SPEED_PTR              (0)     // 目標X方向速度 (int16_t)
#define TARGET_Y_SPEED_PTR              (2)     // 目標Y方向速度 (int16_t)
#define TARGET_THETA_SPEED_PTR          (4)     // 目標旋回速度  (int16_t)
#define RECV_BODY_ENABLE_7_14_PTR       (6)     // enable_7_14         (uint8_t)
#define RECV_BODY_MODE_SWITCH_PTR       (7)     // mode_switch         (uint8_t)
#define RECV_BODY_EMERGENCY_DECEL_PTR   (8)     // emergency_decel     (uint8_t)
#define RECV_BODY_RESET_CTRL_ERR_PTR    (9)     // reset_ctrl_error    (uint8_t)
#define RECV_BODY_RESET_MD_ERR_PTR      (10)    // reset_md_error      (uint8_t)
#define RECV_BODY_HEADLIGHT_CMD_PTR     (11)    // headlight_control   (uint8_t)
#define RECV_BODY_TOWERLIGHT_CMD_PTR    (12)    // towerlight_control  (uint8_t)
#define RECV_BODY_BUMPER_CONFIG_CMD_PTR (13)    // bumper_config       (uint8_t)
#define RECV_BODY_BRAKE_CONFIG_CMD_PTR  (14)    // brake_config        (uint8_t)
#define RECV_BODY_PRODUCT_ID_PTR        (60)    // プロダクトID  (uint16_t)
#define RECV_BODY_ROBOT_ID_PTR          (62)    // ロボットID    (uint16_t)

// ----------------------------------------------------------------------------
// 送信ボディ内オフセット (RPi→PC)
// ----------------------------------------------------------------------------
#define SEND_X_SPEED_PTR            (0)     // 現在X方向速度       (int16_t)
#define SEND_Y_SPEED_PTR            (2)     // 現在Y方向速度       (int16_t)
#define SEND_THETA_SPEED_PTR        (4)     // 現在旋回速度        (int16_t)
#define SEND_CTRL_STATUS_PTR        (6)     // controller_status   (uint8_t)
#define SEND_CTRL_ERR_PTR           (7)     // controller_error    (uint8_t)
#define SEND_MD_ERR_PTR             (8)     // motordriver_error   (uint8_t)
#define SEND_DRIVER_VOLTAGE_PTR     (9)     // driver_voltage_raw  (uint16_t)
#define SEND_HEADLIGHT_PTR          (11)    // headlight_status    (uint8_t)
#define SEND_TOWERLIGHT_PTR         (12)    // towerlight_status   (uint8_t)
#define SEND_IO_INPUT_PTR           (13)    // io_input_status     (uint8_t)
#define SEND_ENCODER0_PTR           (14)    // encoder_motor0      (uint32_t)
#define SEND_ENCODER1_PTR           (18)    // encoder_motor1      (uint32_t)
#define SEND_ENCODER2_PTR           (22)    // encoder_motor2      (uint32_t)
#define SEND_ENCODER3_PTR           (26)    // encoder_motor3      (uint32_t)
#define SEND_MD_TEMP0_PTR           (30)    // motordriver_temp0   (uint16_t)
#define SEND_MD_TEMP1_PTR           (32)    // motordriver_temp1   (uint16_t)
#define SEND_MD_TEMP2_PTR           (34)    // motordriver_temp2   (uint16_t)
#define SEND_MD_TEMP3_PTR           (36)    // motordriver_temp3   (uint16_t)
#define SEND_MD_ERR_CODE0_PTR       (38)    // motordriver_error_code0 (uint16_t)
#define SEND_MD_ERR_CODE1_PTR       (40)    // motordriver_error_code1 (uint16_t)
#define SEND_MD_ERR_CODE2_PTR       (42)    // motordriver_error_code2 (uint16_t)
#define SEND_MD_ERR_CODE3_PTR       (44)    // motordriver_error_code3 (uint16_t)
#define SEND_BUMPER_CONFIG_PTR      (46)    // bumper_config       (uint8_t)
#define SEND_BRAKE_CONFIG_PTR       (47)    // brake_config        (uint8_t)
#define SEND_PRODUCT_ID_PTR         (60)    // プロダクトID        (uint16_t)
#define SEND_ROBOT_ID_PTR           (62)    // ロボットID          (uint16_t)

// ----------------------------------------------------------------------------
// バッファ読み書きヘルパー (内部使用)
// ----------------------------------------------------------------------------

static void WriteInt16ToBuf(uint8_t *buf, int offset, int16_t val) {
	memcpy(buf + offset, &val, sizeof(int16_t));
}

static void WriteUint8ToBuf(uint8_t *buf, int offset, uint8_t val) {
	memcpy(buf + offset, &val, sizeof(uint8_t));
}

static void WriteUint16ToBuf(uint8_t *buf, int offset, uint16_t val) {
	memcpy(buf + offset, &val, sizeof(uint16_t));
}

static void WriteUint32ToBuf(uint8_t *buf, int offset, uint32_t val) {
	memcpy(buf + offset, &val, sizeof(uint32_t));
}

static int16_t ReadInt16FromBody(const uint8_t *bodyPtr, int offset) {
	int16_t val;
	memcpy(&val, bodyPtr + offset, sizeof(int16_t));
	return val;
}

static uint16_t ReadUint16FromBody(const uint8_t *bodyPtr, int offset) {
	uint16_t val;
	memcpy(&val, bodyPtr + offset, sizeof(uint16_t));
	return val;
}

static uint16_t ReadUint16FromHeader(const uint8_t *buf, int offset) {
	if (offset >= SERIAL_HEADER_SIZE - 1) {
		return 0;
	}
	uint16_t val;
	memcpy(&val, buf + offset, sizeof(uint16_t));
	return val;
}

// ----------------------------------------------------------------------------
// チェックサム計算 (IPチェックサムアルゴリズム、リトルエンディアン)
// bodyデータのみを対象に計算する
// ----------------------------------------------------------------------------
static uint16_t CalcChecksum(const uint8_t *data, size_t size) {
	uint32_t sum = 0;
	for (size_t i = 0; i < size; i += 2) {
		uint16_t word = (static_cast<uint16_t>(data[i + 1]) << 8) | static_cast<uint16_t>(data[i]);
		sum += word;
	}
	if (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return static_cast<uint16_t>(~sum);
}

// ----------------------------------------------------------------------------
// 受信パケット解析・検証
// ----------------------------------------------------------------------------
bool RosCommParsePacket(const uint8_t *buffer, size_t size, RosCommRecvData *outData, bool handshakeDone) {
#ifdef DEBUG_WIFI_RX_LOG
	Serial.print("[WiFi RX] ");
	for (size_t i = 0; i < size; i++) {
		if (buffer[i] < 0x10) Serial.print('0');
		Serial.print(buffer[i], HEX);
		Serial.print(' ');
	}
	Serial.println();
#endif
#ifdef DEBUG_BOX_CN_RX_LOG
	Serial.print("[BOX_CN RX] ");
	for (size_t i = 0; i < size; i++) {
		if (buffer[i] < 0x10) Serial.print('0');
		Serial.print(buffer[i], HEX);
		Serial.print(' ');
	}
	Serial.println();
#endif
#ifdef DEBUG_BT_RX_LOG
	Serial.print("[BT RX] ");
	for (size_t i = 0; i < size; i++) {
		if (buffer[i] < 0x10) Serial.print('0');
		Serial.print(buffer[i], HEX);
		Serial.print(' ');
	}
	Serial.println();
#endif

	if (size < (size_t)(SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE)) {
		return false;
	}

	const uint8_t *bodyPtr = buffer + SERIAL_HEADER_SIZE;

	// ヘッダとボディのID整合性チェック
	uint16_t headerProductId = ReadUint16FromHeader(buffer, RECV_HEADER_PRODUCT_ID_PTR);
	uint16_t headerRobotId   = ReadUint16FromHeader(buffer, RECV_HEADER_ROBOT_ID_PTR);
	uint16_t bodyProductId   = ReadUint16FromBody(bodyPtr, RECV_BODY_PRODUCT_ID_PTR);
	uint16_t bodyRobotId     = ReadUint16FromBody(bodyPtr, RECV_BODY_ROBOT_ID_PTR);
	if (headerProductId != bodyProductId || headerRobotId != bodyRobotId) {
		return false;
	}

	// プロトコル識別子チェック (product_idの10000の位が一致すること)
	uint16_t productId    = headerProductId;
	uint16_t recvChecksum = ReadUint16FromHeader(buffer, RECV_HEADER_CHECKSUM_PTR);

	if ((productId / 10000U) != (unsigned int)PROTOCOL_ID) {
		return false;
	}

	// チェックサム検証 (ボディ部分のみ)
	if (recvChecksum != CalcChecksum(bodyPtr, SERIAL_BIN_BUFF_SIZE)) {
		return false;
	}

	// イニシャル要求判定: ハンドシェイク済みの場合はスキップし、常に速度指令として扱う
	if (handshakeDone) {
		outData->isInitialReq = false;
	} else {
		// body[0-59]が全て0のときイニシャル要求とみなす
		outData->isInitialReq = true;
		for (int i = 0; i < INITIAL_REQ_CHECK_LEN; i++) {
			if (bodyPtr[i] != 0x00) {
				outData->isInitialReq = false;
				break;
			}
		}
	}

	if (!outData->isInitialReq) {
		outData->xSpeed   = ReadInt16FromBody(bodyPtr, TARGET_X_SPEED_PTR);
		outData->ySpeed   = ReadInt16FromBody(bodyPtr, TARGET_Y_SPEED_PTR);
		outData->yawSpeed = ReadInt16FromBody(bodyPtr, TARGET_THETA_SPEED_PTR);

		outData->enable_7_14             = bodyPtr[RECV_BODY_ENABLE_7_14_PTR];
		outData->mode_switch             = bodyPtr[RECV_BODY_MODE_SWITCH_PTR];
		outData->emergency_decel         = bodyPtr[RECV_BODY_EMERGENCY_DECEL_PTR];
		outData->reset_controller_error  = bodyPtr[RECV_BODY_RESET_CTRL_ERR_PTR];
		outData->reset_motordriver_error = bodyPtr[RECV_BODY_RESET_MD_ERR_PTR];
		outData->headlight_control       = bodyPtr[RECV_BODY_HEADLIGHT_CMD_PTR];
		outData->towerlight_control      = bodyPtr[RECV_BODY_TOWERLIGHT_CMD_PTR];
		outData->bumper_config           = bodyPtr[RECV_BODY_BUMPER_CONFIG_CMD_PTR];
		outData->brake_config            = bodyPtr[RECV_BODY_BRAKE_CONFIG_CMD_PTR];
	} else {
		outData->xSpeed   = outData->ySpeed = outData->yawSpeed = 0;
		outData->enable_7_14             = 0;
		outData->mode_switch             = 0;
		outData->emergency_decel         = 0;
		outData->reset_controller_error  = 0;
		outData->reset_motordriver_error = 0;
		outData->headlight_control       = 0;
		outData->towerlight_control      = 0;
		outData->bumper_config           = 0;
		outData->brake_config            = 0;
	}

	return true;
}

// ----------------------------------------------------------------------------
// 応答パケット送信
// ----------------------------------------------------------------------------
void RosCommSendResponse(PacketSerial *ps, const RosCommSendData *data, bool isInitialReq) {
	uint8_t sendBody[SERIAL_BIN_BUFF_SIZE];
	memset(sendBody, 0, sizeof(sendBody));

	if (!isInitialReq) {
		WriteInt16ToBuf(sendBody, SEND_X_SPEED_PTR,     data->curX);
		WriteInt16ToBuf(sendBody, SEND_Y_SPEED_PTR,     data->curY);
		WriteInt16ToBuf(sendBody, SEND_THETA_SPEED_PTR, data->curYaw);

		WriteUint8ToBuf (sendBody, SEND_CTRL_STATUS_PTR,    data->controller_status);
		WriteUint8ToBuf (sendBody, SEND_CTRL_ERR_PTR,       data->controller_error);
		WriteUint8ToBuf (sendBody, SEND_MD_ERR_PTR,         data->motordriver_error);
		WriteUint16ToBuf(sendBody, SEND_DRIVER_VOLTAGE_PTR, data->driver_voltage_raw);
		WriteUint8ToBuf (sendBody, SEND_HEADLIGHT_PTR,      data->headlight_status);
		WriteUint8ToBuf (sendBody, SEND_TOWERLIGHT_PTR,     data->towerlight_status);
		WriteUint8ToBuf (sendBody, SEND_IO_INPUT_PTR,       data->io_input_status);
		WriteUint32ToBuf(sendBody, SEND_ENCODER0_PTR,       data->encoder_motor[0]);
		WriteUint32ToBuf(sendBody, SEND_ENCODER1_PTR,       data->encoder_motor[1]);
		WriteUint32ToBuf(sendBody, SEND_ENCODER2_PTR,       data->encoder_motor[2]);
		WriteUint32ToBuf(sendBody, SEND_ENCODER3_PTR,       data->encoder_motor[3]);
		WriteUint16ToBuf(sendBody, SEND_MD_TEMP0_PTR,       data->motordriver_temp[0]);
		WriteUint16ToBuf(sendBody, SEND_MD_TEMP1_PTR,       data->motordriver_temp[1]);
		WriteUint16ToBuf(sendBody, SEND_MD_TEMP2_PTR,       data->motordriver_temp[2]);
		WriteUint16ToBuf(sendBody, SEND_MD_TEMP3_PTR,       data->motordriver_temp[3]);
		WriteUint16ToBuf(sendBody, SEND_MD_ERR_CODE0_PTR,   data->motordriver_error_code[0]);
		WriteUint16ToBuf(sendBody, SEND_MD_ERR_CODE1_PTR,   data->motordriver_error_code[1]);
		WriteUint16ToBuf(sendBody, SEND_MD_ERR_CODE2_PTR,   data->motordriver_error_code[2]);
		WriteUint16ToBuf(sendBody, SEND_MD_ERR_CODE3_PTR,   data->motordriver_error_code[3]);
		WriteUint8ToBuf (sendBody, SEND_BUMPER_CONFIG_PTR,  data->bumper_config);
		WriteUint8ToBuf (sendBody, SEND_BRAKE_CONFIG_PTR,   data->brake_config);
	}
	WriteUint16ToBuf(sendBody, SEND_PRODUCT_ID_PTR, (uint16_t)PRODUCT_ID);
	WriteUint16ToBuf(sendBody, SEND_ROBOT_ID_PTR,   (uint16_t)ROBOT_ID);

	uint16_t sendChecksum = CalcChecksum(sendBody, SERIAL_BIN_BUFF_SIZE);
	uint16_t sendLen = SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE;
	uint16_t sendHeader[4] = {
		(uint16_t)PRODUCT_ID,
		(uint16_t)ROBOT_ID,
		sendLen,
		sendChecksum
	};

	uint8_t sendPacket[SERIAL_HEADER_SIZE + SERIAL_BIN_BUFF_SIZE];
	memcpy(sendPacket, sendHeader, SERIAL_HEADER_SIZE);
	memcpy(sendPacket + SERIAL_HEADER_SIZE, sendBody, SERIAL_BIN_BUFF_SIZE);

#ifdef DEBUG_WIFI_TX_LOG
	Serial.print("[WiFi TX] ");
	for (size_t i = 0; i < sendLen; i++) {
		if (sendPacket[i] < 0x10) Serial.print('0');
		Serial.print(sendPacket[i], HEX);
		Serial.print(' ');
	}
	Serial.println();
#endif
#ifdef DEBUG_BOX_CN_TX_LOG
	Serial.print("[BOX_CN TX] ");
	for (size_t i = 0; i < sendLen; i++) {
		if (sendPacket[i] < 0x10) Serial.print('0');
		Serial.print(sendPacket[i], HEX);
		Serial.print(' ');
	}
	Serial.println();
#endif
#ifdef DEBUG_BT_TX_LOG
	Serial.print("[BT TX] ");
	for (size_t i = 0; i < sendLen; i++) {
		if (sendPacket[i] < 0x10) Serial.print('0');
		Serial.print(sendPacket[i], HEX);
		Serial.print(' ');
	}
	Serial.println();
#endif

	ps->send(sendPacket, sendLen);
}
