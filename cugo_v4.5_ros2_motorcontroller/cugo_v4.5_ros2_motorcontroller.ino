// CuGo V4.5 ROS2モータコントローラ
//
// 概要:
//   cugo_v4.5_ros2_control (ROSコントローラノード) と通信し、
//   CRST01A車両コントローラを制御するArduinoプログラム。
//   PacketSerialライブラリによりCOBSエンコードされた速度指令を受信し、
//   CRST01Aへ転送する。現在速度をROSへ返答する。
//
// 対象ボード: Raspberry Pi Pico 2WH
//
// 通信ポート:
//   Serial  (USB CDC) : ROSコントローラノードとの通信 (PacketSerial使用)
//   Serial1 (UART0)   : CRST01A車両コントローラとの通信 (crst01a_arduino_lib使用)
//
// 使用ライブラリ:
//   PacketSerial        : COBSエンコード/デコード
//   crst01a_arduino_lib : CRST01A操作SDK
//
// プロトコル: ロボット-ROS通信仕様 プロトコル識別子1 (プロダクトID 10000)
#include <PacketSerial.h>
#include "Crst01a.h"
#include "RosComm.h"

// ----------------------------------------------------------------------------
// CRST01A定数
// ----------------------------------------------------------------------------
// CRST01A初期化待機時間 (ms)
#define CRST_INIT_WAIT_MS               (200)

// 走行状態 (0x81) データの有効期限 (ms)
#define CRST_RUN_STATUS_TIMEOUT_MS      (500)

// システムステータス (0x80) データの有効期限 (ms)
#define CRST_SYS_STATUS_TIMEOUT_MS      (1000)

// コントローラエラービット (0x80 controller error)
#define CRST_ERR_CTL_SERIAL_TIMEOUT     (0x40)  // bit6: シリアルタイムアウトエラー
// 非常停止関連エラービット (CRST01A側で停止を制御するため、速度制限対象外とする)
// bit3: 緊急減速実行後, bit4: 非常停止スイッチラッチ
#define CRST_ERR_CTL_ESTOP_MASK         (0x18)

// コントローラステータスビット (0x80 controller status)
#define CRST_STS_CMD_MODE               (0x01)  // bit0: コマンドモード

// ----------------------------------------------------------------------------
// プロトタイプ
// ----------------------------------------------------------------------------

void CheckFailsafe(void);
void MonitorCrstStatus(void);
void OnSerialPacketReceived(const uint8_t *buffer, size_t size);

// ----------------------------------------------------------------------------
// 定周期ジョブ
// ----------------------------------------------------------------------------


void Job10ms(void) {
	// 予約
}

void Job100ms(void) {
	CheckFailsafe();
	MonitorCrstStatus();
}

void Job1000ms(void) {
	// 予約
}

// ----------------------------------------------------------------------------
// フェイルセーフ
// 5回連続 (100ms × 5 = 0.5秒) ROSからの通信が来なかった場合に停止する
// ----------------------------------------------------------------------------
#define FAILSAFE_COUNT_MAX  (5)

PacketSerial packetSerial;

// タイムスタンプ (オーバーフローしても符号なし演算で正常に動作する)
unsigned long long currentTime = 0;
unsigned long long prevTime10ms = 0;
unsigned long long prevTime100ms = 0;
unsigned long long prevTime1000ms = 0;

// 通信失敗カウンタ
int comFailCount = 0;

// フェイルセーフ状態フラグ (通信断によるフェイルセーフ発動中)
bool failsafeActiveF = false;

// ハンドシェイク完了フラグ
// イニシャル要求の正常受信後にtrueになり、通信断・通信エラー時にfalseに戻る
bool handshakeDoneF = false;

// ----------------------------------------------------------------------------
// CRST01Aエラー判定
// 非常停止関連(緊急減速実行後, 非常停止スイッチラッチ)を除くエラーを検出する
// 戻り値: 走行を阻害するエラーが発生している場合はtrue
// ----------------------------------------------------------------------------
bool IsCrstErrorActive(uint8_t ctrlErr, uint8_t drvErr) {
	return ((ctrlErr & ~CRST_ERR_CTL_ESTOP_MASK) != 0) || (drvErr != 0);
}

// ----------------------------------------------------------------------------
// フェイルセーフ
// ----------------------------------------------------------------------------

// 100msごとに呼び出し、通信断を検知したらロボットを停止する
void CheckFailsafe(void) {
	comFailCount++;
	if (comFailCount > FAILSAFE_COUNT_MAX) {
		if (!failsafeActiveF) {
			failsafeActiveF = true;
			handshakeDoneF = false;  // 通信断のためハンドシェイクをリセット
		}
		crst01a.SetMoveSpeed(0, 0, 0);
	}
}

// ----------------------------------------------------------------------------
// CRST01A状態監視
// 100msごとに呼び出し、CRST01Aの接続状態とエラー状態を確認する
// ----------------------------------------------------------------------------
void MonitorCrstStatus(void) {
	uint8_t ctrlStatus, ctrlErr, drvErr;
	uint16_t drvVoltage;
	uint32_t recvTime;
	crst01a.GetSysStatus(&ctrlStatus, &ctrlErr, &drvErr, &drvVoltage, &recvTime);

	// 接続断を検知したら停止
	bool isConnected = (millis() - recvTime < CRST_SYS_STATUS_TIMEOUT_MS);
	if (!isConnected) {
		crst01a.SetMoveSpeed(0, 0, 0);
		return;
	}

	// 非常停止を除くエラーが発生した場合、速度を0に設定
	if (IsCrstErrorActive(ctrlErr, drvErr)) {
		crst01a.SetMoveSpeed(0, 0, 0);
	}
}

// ----------------------------------------------------------------------------
// パケット受信コールバック (PacketSerialより呼び出し)
// ----------------------------------------------------------------------------
void OnSerialPacketReceived(const uint8_t *buffer, size_t size) {
	RosCommRecvData recvData;
	if (!RosCommParsePacket(buffer, size, &recvData, handshakeDoneF)) {
		// プロトコルエラー・チェックサム不一致: 応答なし
		// ハンドシェイク状態はリセットしない (再接続時の残留データで誤リセットを防ぐ)
		// ハンドシェイクのリセットはフェイルセーフ (通信タイムアウト) でのみ行う
		return;
	}

	// フェイルセーフ復帰処理: 通信が回復したらシリアルタイムアウトエラーをクリア
	if (failsafeActiveF) {
		crst01a.ClearControllerError(CRST_ERR_CTL_SERIAL_TIMEOUT);
		failsafeActiveF = false;
	}

	// 有効な通信を受信したのでフェイルセーフカウンタをリセット
	comFailCount = 0;

	// ハンドシェイク未完了の場合
	if (!handshakeDoneF) {
		if (!recvData.isInitialReq) {
			// イニシャル要求以外は無視: 速度0を設定し、応答は返さない
			crst01a.SetMoveSpeed(0, 0, 0);
			return;
		}
		// イニシャル要求を受理: ハンドシェイク完了
		handshakeDoneF = true;
		crst01a.SetMoveSpeed(0, 0, 0);
		crst01a.SetControlMode(CRST_CMD_MODE);  // ハンドシェイク後に1度だけCMDモードへ移行
		RosCommSendResponse(&packetSerial, 0, 0, 0, true);
		return;
	}

	// ハンドシェイク済み: 速度指令を処理
	uint8_t ctrlStatus, ctrlErr, drvErr;
	uint16_t drvVoltage;
	uint32_t sysRecvTime;
	crst01a.GetSysStatus(&ctrlStatus, &ctrlErr, &drvErr, &drvVoltage, &sysRecvTime);

	bool isErr = (millis() - sysRecvTime < CRST_SYS_STATUS_TIMEOUT_MS);

	// 非常停止を除くエラーがない場合のみ速度指令を転送する
	if (isErr && !IsCrstErrorActive(ctrlErr, drvErr)) {
		crst01a.SetMoveSpeed(recvData.xSpeed, recvData.ySpeed, recvData.yawSpeed);
	} else {
		crst01a.SetMoveSpeed(0, 0, 0);
	}

	// 現在速度取得 (データが有効期限切れの場合は全て0を返す)
	int16_t curX = 0, curY = 0, curYaw = 0;
	uint32_t runRecvTime;
	crst01a.GetReadRunStatus(&curX, &curY, &curYaw, &runRecvTime);
	if (millis() - runRecvTime >= CRST_RUN_STATUS_TIMEOUT_MS) {
		curX = curY = curYaw = 0;
	}

	RosCommSendResponse(&packetSerial, curX, curY, curYaw, false);
}

// ----------------------------------------------------------------------------
// 初期化
// ----------------------------------------------------------------------------
void setup() {
	// CRST01A初期化 (CMDモードへの移行はハンドシェイク後に行う)
	crst01a.Init();
	delay(CRST_INIT_WAIT_MS);

	// システムステータス (0x80) の定期受信を有効化 (50Hz)
	crst01a.SetCycleReq(CRST_FUNC_READ_SYS_STATUS);

	// 走行状態 (0x81) の定期受信を有効化 (50Hz)
	crst01a.SetCycleReq(CRST_FUNC_READ_RUN_STATUS);

	// PacketSerial初期化 (COBSエンコード/デコード、USB CDC Serial使用)
	packetSerial.begin(115200);
	packetSerial.setStream(&Serial);
	packetSerial.setPacketHandler(&OnSerialPacketReceived);

	// 起動直後のシリアルバッファをクリア
	delay(100);
	while (Serial.available() > 0) {
		Serial.read();
	}
}

// ----------------------------------------------------------------------------
// メインループ
// ----------------------------------------------------------------------------
void loop() {
	currentTime = micros();

	if (currentTime - prevTime10ms > 10000) {
		Job10ms();
		prevTime10ms = currentTime;
	}

	if (currentTime - prevTime100ms > 100000) {
		Job100ms();
		prevTime100ms = currentTime;
	}

	if (currentTime - prevTime1000ms > 1000000) {
		Job1000ms();
		prevTime1000ms = currentTime;
	}

	// PacketSerial受信処理 (受信完了でOnSerialPacketReceivedを呼び出す)
	packetSerial.update();

	// バッファオーバーフロー検知 (必要に応じて対処)
	if (packetSerial.overflow()) {
		// 現状は無視 (必要に応じてエラー通知等を追加)
	}
}
