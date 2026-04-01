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
		RosCommSendResponse(&transport.serial(), 0, 0, 0, true);
		return;
	}

	// ハンドシェイク済み: 速度指令を処理
	uint8_t ctrlStatus, ctrlErr, drvErr;
	uint16_t drvVoltage;
	uint32_t sysRecvTime;
	crst01a.GetSysStatus(&ctrlStatus, &ctrlErr, &drvErr, &drvVoltage, &sysRecvTime);

	bool isConnected = (millis() - sysRecvTime < CRST_SYS_STATUS_TIMEOUT_MS);

	// 非常停止を除くエラーがない場合のみ速度指令を転送する
	if (isConnected && !IsCrstErrorActive(ctrlErr, drvErr)) {
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

	RosCommSendResponse(&transport.serial(), curX, curY, curYaw, false);
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

	// システムステータス (0x80) の定期受信を有効化 (50Hz)
	crst01a.SetCycleReq(CRST_FUNC_READ_SYS_STATUS);

	// 走行状態 (0x81) の定期受信を有効化 (50Hz)
	crst01a.SetCycleReq(CRST_FUNC_READ_RUN_STATUS);

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
