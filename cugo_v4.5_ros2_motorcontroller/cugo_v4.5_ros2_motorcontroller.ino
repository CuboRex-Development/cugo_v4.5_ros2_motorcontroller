// CuGo V4.5 ROS 2 モータコントローラ
//
// 概要:
//   cugo_v4.5_ros2_control (ROS 2 コントローラノード) と通信し、
//   CRST01A車両コントローラを制御するArduinoプログラム。
//   PacketSerialライブラリによりCOBSエンコードされた速度指令を受信し、
//   CRST01Aへ転送する。現在速度をROS 2 へ返答する。
//
// 対象ボード: Raspberry Pi Pico 2 W
//
// 通信ポート:
//   USB-Serial モード: Serial (USB CDC)              — ROSコントローラノードとの通信
//   BOX_CN モード:     Serial2/UART1 (GP8:TX/GP9:RX) — ROSコントローラノードとの通信
//                      基板上のボックスコネクタにあるUARTピンを使用するモード
//   WiFi モード:       WiFi (TCP)                    — ROSコントローラノードとの通信
//   (config.h の USE_BOX_CN / USE_WIFI で切り替え)
//   Serial1 (UART0): CRST01A車両コントローラとの通信 (crst01a_arduino_lib使用)
//
// 使用ライブラリ:
//   PacketSerial        : COBSエンコード/デコード
//   crst01a_arduino_lib : CRST01A操作SDK
//
// プロトコル: ロボット-ROS通信仕様 プロトコル識別子1 (プロダクトID 10000)

#include "config.h"
#include "Transport.h"
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

// 周期データ (0x84, 0x88, 0x89, 0x8C, 0x8E) の有効期限 (ms)
#define CRST_PERIODIC_TIMEOUT_MS        (1000)

// バンパー・ブレーキ設定 (0xC4) データの有効期限 (ms) (1秒ごとに要求するため2倍の余裕)
#define CRST_BUMPER_BRAKE_TIMEOUT_MS    (2000)

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
void onNewWifiClient(void);
#ifdef DEBUG_SERIAL_STATS
void PrintSerialStats(void);
#endif

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
#ifdef DEBUG_SERIAL_STATS
	PrintSerialStats();
#endif
	// バンパー・ブレーキ設定を1秒ごとに非ブロッキングで要求する
	crst01a.GetBumperBrakeReq();
}

// ----------------------------------------------------------------------------
// グローバル変数
// ----------------------------------------------------------------------------

Transport transport;

// タイムスタンプ (オーバーフローしても符号なし演算で正常に動作する)
unsigned long currentTime = 0;
unsigned long prevTime10ms = 0;
unsigned long prevTime100ms = 0;
unsigned long prevTime1000ms = 0;

// 通信失敗カウンタ (フェイルセーフ用)
int comFailCount = 0;

// ループ統計 (DEBUG_SERIAL_STATS 有効時のみ使用)
#ifdef DEBUG_SERIAL_STATS
uint32_t statsLoopCount   = 0;  // 1秒間のループ実行回数
uint32_t statsLoopMaxUs   = 0;  // ループ1回の最大所要時間 [µs]
uint32_t statsUpdateMaxUs = 0;  // transport.update() の最大所要時間 [µs]
uint32_t statsDelayMaxUs  = 0;  // delay(1) の実際の最大所要時間 [µs]
uint32_t statsAvailMax    = 0;  // update() 呼び出し直前の TCP受信バッファの最大バイト数
#endif

// フェイルセーフ状態フラグ (通信断によるフェイルセーフ発動中)
bool failsafeActiveF = false;

// ハンドシェイク完了フラグ
// イニシャル要求の正常受信後にtrueになり、通信断・通信エラー時にfalseに戻る
bool handshakeDoneF = false;

// ライト制御状態キャッシュ (SetLightsが両フィールドを同時に設定するため保持)
uint8_t g_headlightControl  = 0;
uint8_t g_towerlightControl = 0;

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
// 5回連続 (100ms × 5 = 0.5秒) ROSからの通信が来なかった場合に停止する
// ----------------------------------------------------------------------------
#define FAILSAFE_COUNT_MAX  (5)

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
		RosCommSendData sendData;
		memset(&sendData, 0, sizeof(sendData));
		RosCommSendResponse(&transport.serial(), &sendData, true);
		return;
	}

	// ハンドシェイク済み: IO指令を処理 (enable_7_14 の各ビットが1のフィールドのみ反映)

	// モード切替
	if (recvData.enable_7_14 & ENABLE_BIT_MODE_SWITCH) {
		crst01a.SetControlMode(recvData.mode_switch);
	}

	// 緊急減速
	if (recvData.enable_7_14 & ENABLE_BIT_EMERGENCY_DECEL) {
		crst01a.SetEmergencyDeceleration();
	}

	// コントローラエラー解除
	if (recvData.enable_7_14 & ENABLE_BIT_RESET_CTRL_ERROR) {
		crst01a.ClearControllerError(recvData.reset_controller_error);
	}

	// モータドライバエラー解除
	if (recvData.enable_7_14 & ENABLE_BIT_RESET_MD_ERROR) {
		crst01a.ClearDriverError(recvData.reset_motordriver_error);
	}

	// ライト制御 (headlight/towerlightは片方だけ変更する場合があるためキャッシュで補完)
	if ((recvData.enable_7_14 & ENABLE_BIT_HEADLIGHT) ||
	    (recvData.enable_7_14 & ENABLE_BIT_TOWERLIGHT)) {
		if (recvData.enable_7_14 & ENABLE_BIT_HEADLIGHT)  g_headlightControl  = recvData.headlight_control;
		if (recvData.enable_7_14 & ENABLE_BIT_TOWERLIGHT) g_towerlightControl = recvData.towerlight_control;
		crst01a.SetLights(g_headlightControl, g_towerlightControl);
	}

	// バンパー・ブレーキ設定 (変更しないフィールドはGetBumperBrakeResのキャッシュで補完)
	if ((recvData.enable_7_14 & ENABLE_BIT_BUMPER_CONFIG) ||
	    (recvData.enable_7_14 & ENABLE_BIT_BRAKE_CONFIG)) {
		uint8_t curBumper, curBrake;
		uint32_t bbTime;
		crst01a.GetBumperBrakeRes(&curBumper, &curBrake, &bbTime);
		uint8_t newBumper = (recvData.enable_7_14 & ENABLE_BIT_BUMPER_CONFIG) ? recvData.bumper_config : curBumper;
		uint8_t newBrake  = (recvData.enable_7_14 & ENABLE_BIT_BRAKE_CONFIG)  ? recvData.brake_config  : curBrake;
		crst01a.SetBumperBrake(newBumper, newBrake);
	}

	// --------------------------------------------------------------------
	// センサデータ収集
	// --------------------------------------------------------------------

	// システムステータス (0x80)
	uint8_t ctrlStatus = 0, ctrlErr = 0, drvErr = 0;
	uint16_t drvVoltage = 0;
	uint32_t sysRecvTime;
	crst01a.GetSysStatus(&ctrlStatus, &ctrlErr, &drvErr, &drvVoltage, &sysRecvTime);
	bool isConnected = (millis() - sysRecvTime < CRST_SYS_STATUS_TIMEOUT_MS);

	// 非常停止を除くエラーがない場合のみ速度指令を転送する
	if (isConnected && !IsCrstErrorActive(ctrlErr, drvErr)) {
		crst01a.SetMoveSpeed(recvData.xSpeed, recvData.ySpeed, recvData.yawSpeed);
	} else {
		crst01a.SetMoveSpeed(0, 0, 0);
	}

	// 現在速度 (0x81)
	int16_t curX = 0, curY = 0, curYaw = 0;
	uint32_t runRecvTime;
	crst01a.GetReadRunStatus(&curX, &curY, &curYaw, &runRecvTime);
	if (millis() - runRecvTime >= CRST_RUN_STATUS_TIMEOUT_MS) {
		curX = curY = curYaw = 0;
	}

	// 外部IO (0x84): ヘッドライト・タワーライト状態・4bit入力
	uint8_t headlightStatus = 0, towerlightStatus = 0, ioInputStatus = 0;
	uint32_t extIoRecvTime;
	crst01a.GetExtIo(&headlightStatus, &towerlightStatus, &ioInputStatus, &extIoRecvTime);
	if (millis() - extIoRecvTime >= CRST_PERIODIC_TIMEOUT_MS) {
		headlightStatus = towerlightStatus = ioInputStatus = 0;
	}

	// エンコーダ (0x88, 0x89)
	uint32_t encoderMotor[4] = {0, 0, 0, 0};
	uint32_t encoderRecvTime;
	crst01a.GetEncoder(encoderMotor, &encoderRecvTime);
	if (millis() - encoderRecvTime >= CRST_PERIODIC_TIMEOUT_MS) {
		memset(encoderMotor, 0, sizeof(encoderMotor));
	}

	// モータドライバ温度 (0x8C)
	uint16_t mdTemp[4] = {0, 0, 0, 0};
	uint32_t mdTempRecvTime;
	crst01a.GetMdTemp(mdTemp, &mdTempRecvTime);
	if (millis() - mdTempRecvTime >= CRST_PERIODIC_TIMEOUT_MS) {
		memset(mdTemp, 0, sizeof(mdTemp));
	}

	// モータドライバエラーコード (0x8E)
	uint16_t mdErrCode[4] = {0, 0, 0, 0};
	uint32_t mdStatusRecvTime;
	crst01a.GetMdStatus(mdErrCode, &mdStatusRecvTime);
	if (millis() - mdStatusRecvTime >= CRST_PERIODIC_TIMEOUT_MS) {
		memset(mdErrCode, 0, sizeof(mdErrCode));
	}

	// バンパー・ブレーキ設定 (0xC4, Job1000msで要求済みのキャッシュを使用)
	uint8_t bumperConfig = 0, brakeConfig = 0;
	uint32_t bumperBrakeRecvTime;
	crst01a.GetBumperBrakeRes(&bumperConfig, &brakeConfig, &bumperBrakeRecvTime);
	if (bumperBrakeRecvTime == 0 || millis() - bumperBrakeRecvTime >= CRST_BUMPER_BRAKE_TIMEOUT_MS) {
		bumperConfig = brakeConfig = 0;
	}

	// --------------------------------------------------------------------
	// 送信データ構築・送信
	// --------------------------------------------------------------------
	RosCommSendData sendData;
	sendData.curX                = curX;
	sendData.curY                = curY;
	sendData.curYaw              = curYaw;
	sendData.controller_status   = ctrlStatus;
	sendData.controller_error    = ctrlErr;
	sendData.motordriver_error   = drvErr;
	sendData.driver_voltage_raw  = drvVoltage;
	sendData.headlight_status    = headlightStatus;
	sendData.towerlight_status   = towerlightStatus;
	sendData.io_input_status     = ioInputStatus;
	for (int i = 0; i < 4; i++) {
		sendData.encoder_motor[i]          = encoderMotor[i];
		sendData.motordriver_temp[i]       = mdTemp[i];
		sendData.motordriver_error_code[i] = mdErrCode[i];
	}
	sendData.bumper_config = bumperConfig;
	sendData.brake_config  = brakeConfig;

	RosCommSendResponse(&transport.serial(), &sendData, false);
}

// ----------------------------------------------------------------------------
// シリアル統計出力 (DEBUG_SERIAL_STATS 有効時のみ使用)
// ----------------------------------------------------------------------------
#ifdef DEBUG_SERIAL_STATS
void PrintSerialStats(void) {
	Serial.print("[STATS] loops/s=");  Serial.print(statsLoopCount);
	Serial.print("  maxLoop=");        Serial.print(statsLoopMaxUs);   Serial.print("us");
	Serial.print("  maxUpdate=");      Serial.print(statsUpdateMaxUs); Serial.print("us");
	Serial.print("  maxDelay1=");      Serial.print(statsDelayMaxUs);  Serial.print("us");
	Serial.print("  maxAvail=");       Serial.print(statsAvailMax);    Serial.println("B");
	statsLoopCount   = 0;
	statsLoopMaxUs   = 0;
	statsUpdateMaxUs = 0;
	statsDelayMaxUs  = 0;
	statsAvailMax    = 0;
}
#endif

// ----------------------------------------------------------------------------
// WiFi 新規クライアント接続時コールバック
// Transport::update() から呼び出される (Serial モードでは呼び出されない)
// ----------------------------------------------------------------------------
void onNewWifiClient(void) {
	handshakeDoneF  = false;
	failsafeActiveF = false;
	comFailCount    = 0;
}

// ----------------------------------------------------------------------------
// 初期化
// ----------------------------------------------------------------------------
void setup() {
	// CRST01A初期化 (CMDモードへの移行はハンドシェイク後に行う)
	crst01a.Init();
	delay(CRST_INIT_WAIT_MS);

	// 定期送信コマンドの送信周期を50Hzに設定
	crst01a.SetCycleReqFrequency(0x02);

	// システムステータス (0x80) の定期受信を有効化
	crst01a.SetCycleReq(CRST_FUNC_READ_SYS_STATUS);

	// 走行状態 (0x81) の定期受信を有効化
	crst01a.SetCycleReq(CRST_FUNC_READ_RUN_STATUS);

	// 外部IO (0x84) の定期受信を有効化
	crst01a.SetCycleReq(CRST_FUNC_READ_EXT_IO);

	// モータエンコーダ (0x88, 0x89) の定期受信を有効化
	crst01a.SetCycleReq(CRST_FUNC_READ_ENCODER_01);
	crst01a.SetCycleReq(CRST_FUNC_READ_ENCODER_23);

	// モータドライバ温度 (0x8C) の定期受信を有効化
	crst01a.SetCycleReq(CRST_FUNC_READ_MD_TEMP);

	// モータドライバ状態 (0x8E) の定期受信を有効化
	crst01a.SetCycleReq(CRST_FUNC_READ_MD_STATUS);

	// トランスポート初期化 (config.h の 設定 に応じて ポートをひらく)
	transport.begin(&OnSerialPacketReceived);
}

// ----------------------------------------------------------------------------
// メインループ
// ----------------------------------------------------------------------------
void loop() {
#ifdef DEBUG_SERIAL_STATS
	uint32_t _loopStart = micros();
#endif

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

	// トランスポート受信処理 (WiFiモードはクライアント管理も含む)
#ifdef DEBUG_SERIAL_STATS
	{
		uint32_t _avail = (uint32_t)transport.clientAvailable();
		if (_avail > statsAvailMax) statsAvailMax = _avail;
		uint32_t _t = micros();
		transport.update(&onNewWifiClient);
		uint32_t _d = micros() - _t;
		if (_d > statsUpdateMaxUs) statsUpdateMaxUs = _d;
	}
#else
	transport.update(&onNewWifiClient);
#endif

#ifdef USE_WIFI
	// WiFiスタック (CYW43) にバックグラウンド処理の時間を渡す
	// delay() 内部で yield() -> cyw43_arch_poll() が呼ばれ、ICMP/ARP等が処理される
#ifdef DEBUG_SERIAL_STATS
	{
		uint32_t _t = micros();
		delay(1);
		uint32_t _d = micros() - _t;
		if (_d > statsDelayMaxUs) statsDelayMaxUs = _d;
	}
#else
	delay(1);
#endif
#endif

#ifdef DEBUG_SERIAL_STATS
	statsLoopCount++;
	uint32_t _loopDur = micros() - _loopStart;
	if (_loopDur > statsLoopMaxUs) statsLoopMaxUs = _loopDur;
#endif
}
