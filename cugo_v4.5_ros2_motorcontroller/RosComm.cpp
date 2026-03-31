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
// 受信ボディ内オフセット
// ----------------------------------------------------------------------------
#define TARGET_X_SPEED_PTR          (0)     // 目標X方向速度 (int16_t)
#define TARGET_Y_SPEED_PTR          (2)     // 目標Y方向速度 (int16_t)
#define TARGET_THETA_SPEED_PTR      (4)     // 目標旋回速度  (int16_t)
#define RECV_BODY_PRODUCT_ID_PTR    (60)    // プロダクトID  (uint16_t)
#define RECV_BODY_ROBOT_ID_PTR      (62)    // ロボットID    (uint16_t)

// ----------------------------------------------------------------------------
// 送信ボディ内オフセット
// ----------------------------------------------------------------------------
#define SEND_X_SPEED_PTR        (0)     // 現在X方向速度 (int16_t)
#define SEND_Y_SPEED_PTR        (2)     // 現在Y方向速度 (int16_t)
#define SEND_THETA_SPEED_PTR    (4)     // 現在旋回速度  (int16_t)
#define SEND_PRODUCT_ID_PTR     (60)    // プロダクトID  (uint16_t)
#define SEND_ROBOT_ID_PTR       (62)    // ロボットID    (uint16_t)

// ----------------------------------------------------------------------------
// バッファ読み書きヘルパー (内部使用)
// ----------------------------------------------------------------------------

static void WriteInt16ToBuf(uint8_t *buf, int offset, int16_t val) {
	memcpy(buf + offset, &val, sizeof(int16_t));
}

static void WriteUint16ToBuf(uint8_t *buf, int offset, uint16_t val) {
	memcpy(buf + offset, &val, sizeof(uint16_t));
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
	} else {
		outData->xSpeed = outData->ySpeed = outData->yawSpeed = 0;
	}

	return true;
}

// ----------------------------------------------------------------------------
// 応答パケット送信
// ----------------------------------------------------------------------------
void RosCommSendResponse(PacketSerial *ps, int16_t curX, int16_t curY, int16_t curYaw, bool isInitialReq) {
	uint8_t sendBody[SERIAL_BIN_BUFF_SIZE];
	memset(sendBody, 0, sizeof(sendBody));

	if (!isInitialReq) {
		WriteInt16ToBuf(sendBody, SEND_X_SPEED_PTR, curX);
		WriteInt16ToBuf(sendBody, SEND_Y_SPEED_PTR, curY);
		WriteInt16ToBuf(sendBody, SEND_THETA_SPEED_PTR, curYaw);
	}
	WriteUint16ToBuf(sendBody, SEND_PRODUCT_ID_PTR, (uint16_t)PRODUCT_ID);
	WriteUint16ToBuf(sendBody, SEND_ROBOT_ID_PTR, (uint16_t)ROBOT_ID);

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
