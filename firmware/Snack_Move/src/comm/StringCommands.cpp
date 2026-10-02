/**
 * StringCommands.cpp - BLE 字符串命令路由实现
 */

#include "StringCommands.h"
#include "../app/AppManager.h"       // getSystemMode / MODE_ACTION_GROUP
#include "../hal/RBG.h"              // showXxx LED 指示
#include "../servo/SerialServoControl.h"
#include "../servo/ServoRegistry.h"
#include "../motion/Calibration.h"
#include "../motion/ScanSlots.h"
#include "../motion/movement.h"

// ======================== 内部状态 ========================

static Bluetooth*    s_ble    = nullptr;
static HardwareSerial* s_serial = nullptr;

static GaitParams s_gaitParams;
static volatile int s_pendingGait = GAIT_NONE;

// ======================== 初始化 ========================

void strCmdInit(Bluetooth &ble, HardwareSerial &serial) {
    s_ble    = &ble;
    s_serial = &serial;
}

// ======================== 步态调度 getter ========================

int  getPendingGait()       { return s_pendingGait; }
void clearPendingGait()     { s_pendingGait = GAIT_NONE; }
const GaitParams& getGaitParams() { return s_gaitParams; }

// ======================== 舵机电源快捷操作 ========================

void strCmdLoadAllServos() {
    Serial.println("[电源] 舵机上电");
    LobotSerialServoSetLoad(*s_serial, LOBOT_SERVO_ID_ALL, 1);
}

void strCmdUnloadAllServos() {
    Serial.println("[电源] 舵机掉电");
    LobotSerialServoSetLoad(*s_serial, LOBOT_SERVO_ID_ALL, 0);
}

// ======================== 参数解析 ========================
// VERT:  [amp]_[speed]_[lambda]_[bias]_[otherUnload]
// HORIZ: [amp]_[speed]_[lambda]_[bias]_[otherUnload]
// DUAL:  [ampV]_[ampH]_[spV]_[spH]_[lambdaV]_[lambdaH]_[phase]

static void parseGaitParams(const String &params, uint8_t type) {
    s_gaitParams = GaitParams();

    float vals[7] = {0};
    uint8_t cnt = 0;
    int lastIdx = 0;
    while (cnt < 7) {
        int idx = params.indexOf('_', lastIdx);
        String tok = (idx >= 0) ? params.substring(lastIdx, idx) : params.substring(lastIdx);
        vals[cnt] = tok.toFloat();
        cnt++;
        if (idx < 0) break;
        lastIdx = idx + 1;
    }

    switch (type) {
        case GAIT_VERT:
            if (cnt >= 1) s_gaitParams.ampVert    = vals[0];
            if (cnt >= 2) s_gaitParams.speedVert  = vals[1];
            if (cnt >= 3) s_gaitParams.lambdaVert = vals[2];
            if (cnt >= 4) s_gaitParams.bias       = (int16_t)vals[3];
            if (cnt >= 5) s_gaitParams.otherUnload = (vals[4] > 0.5f);
            break;
        case GAIT_HORIZ:
            if (cnt >= 1) s_gaitParams.ampHoriz   = vals[0];
            if (cnt >= 2) s_gaitParams.speedHoriz = vals[1];
            if (cnt >= 3) s_gaitParams.lambdaHoriz = vals[2];
            if (cnt >= 4) s_gaitParams.bias       = (int16_t)vals[3];
            if (cnt >= 5) s_gaitParams.otherUnload = (vals[4] > 0.5f);
            break;
        case GAIT_DUAL:
            if (cnt >= 1) s_gaitParams.ampVert    = vals[0];
            if (cnt >= 2) s_gaitParams.ampHoriz   = vals[1];
            if (cnt >= 3) s_gaitParams.speedVert  = vals[2];
            if (cnt >= 4) s_gaitParams.speedHoriz = vals[3];
            if (cnt >= 5) s_gaitParams.lambdaVert = vals[4];
            if (cnt >= 6) s_gaitParams.lambdaHoriz = vals[5];
            if (cnt >= 7) s_gaitParams.phaseDiff  = vals[6];
            break;
    }
}

// ======================== 命令分发 ========================

void strCmdDispatch(String command) {

    // ===== 动作组模式下仅允许 HELP / INFO 类无害指令 =====
    if (getSystemMode() == MODE_ACTION_GROUP) {
        s_ble->sendDataToClient("In ACTION_GROUP mode. Use app or exit first.\r\n");
        return;
    }

    // --- 扫描指令 ---
    if (command == "SCAN" || command == "SCAN1") {
        Serial.println("[指令] 扫描舵机 → 槽位1");
        stopGait();
        s_pendingGait = GAIT_NONE;
        scanAndSaveToSlot(*s_serial, SLOT_SCAN1, s_ble);
        showGreen(100);
    }
    else if (command == "SCAN2") {
        Serial.println("[指令] 扫描舵机 → 槽位2");
        stopGait();
        s_pendingGait = GAIT_NONE;
        scanAndSaveToSlot(*s_serial, SLOT_SCAN2, s_ble);
        showGreen(100);
    }
    else if (command == "CALIBRATE") {
        Serial.println("[指令] 校准扫描开始");
        showPurple(100);
        stopGait();
        s_pendingGait = GAIT_NONE;
        calibrateAllServos(*s_serial, s_ble);
        showGreen(100);
    }

    // --- 电源指令 ---
    else if (command == "UNLOAD") {
        Serial.println("[指令] 舵机掉电");
        stopGait();
        s_pendingGait = GAIT_NONE;
        strCmdUnloadAllServos();
        showRed(100);
    }
    else if (command == "LOAD") {
        Serial.println("[指令] 舵机上电");
        stopGait();
        s_pendingGait = GAIT_NONE;
        strCmdLoadAllServos();
        showGreen(100);
    }

    // --- 中心基准切换 ---
    else if (command == "CENTER_CALIB" || command == "CENTER_0") {
        Serial.println("[指令] 切换中心基准 → 校准槽");
        stopGait();
        s_pendingGait = GAIT_NONE;
        loadRegistry(SLOT_CALIB);
        if (reloadCenterFromSlot(SLOT_CALIB, s_ble)) {
            s_ble->sendDataToClient("Center source: CALIB (slot 0)\r\n");
        } else {
            s_ble->sendDataToClient("FAILED: CALIB slot empty\r\n");
        }
        showGreen(100);
    }
    else if (command == "CENTER_SCAN1" || command == "CENTER_1") {
        Serial.println("[指令] 切换中心基准 → 扫描1槽");
        stopGait();
        s_pendingGait = GAIT_NONE;
        loadRegistry(SLOT_SCAN1);
        if (reloadCenterFromSlot(SLOT_SCAN1, s_ble)) {
            s_ble->sendDataToClient("Center source: SCAN1 (slot 1)\r\n");
        } else {
            s_ble->sendDataToClient("FAILED: SCAN1 slot empty\r\n");
        }
        showGreen(100);
    }
    else if (command == "CENTER_SCAN2" || command == "CENTER_2") {
        Serial.println("[指令] 切换中心基准 → 扫描2槽");
        stopGait();
        s_pendingGait = GAIT_NONE;
        loadRegistry(SLOT_SCAN2);
        if (reloadCenterFromSlot(SLOT_SCAN2, s_ble)) {
            s_ble->sendDataToClient("Center source: SCAN2 (slot 2)\r\n");
        } else {
            s_ble->sendDataToClient("FAILED: SCAN2 slot empty\r\n");
        }
        showGreen(100);
    }

    // --- 数据指令 ---
    else if (command == "LIST") {
        Serial.println("[指令] 读取全部已保存数据");
        showCyan(100);
        for (uint8_t s = 0; s < SLOT_COUNT; s++) printRegistry(s);

        String list = "=== Saved Data ===\r\n";
        for (uint8_t s = 0; s < SLOT_COUNT; s++) {
            ServoRegistry *reg = getRegistry(s);
            list += "[" + String(getSlotName(s)) + "] ";
            if (reg->count == 0) {
                list += "empty\r\n";
            } else {
                list += String(reg->count) + " servos:\r\n";
                for (uint8_t i = 0; i < reg->count; i++) {
                    list += "  ID:" + String(reg->servos[i].id);
                    list += " Pos:" + String(reg->servos[i].position) + "\r\n";
                }
            }
        }
        s_ble->sendDataToClient(list);
        showGreen(100);
    }
    else if (command == "CLEAR_EEPROM") {
        Serial.println("[指令] 清除全部 EEPROM");
        stopGait();
        s_pendingGait = GAIT_NONE;
        clearAllEEPROM();
        showBlue(100);
        s_ble->sendDataToClient("All EEPROM slots cleared.\r\n");
    }

    // --- 摆位指令 ---
    else if (command == "POS1") {
        Serial.println("[指令] 摆到位姿1");
        stopGait();
        s_pendingGait = GAIT_NONE;
        strCmdLoadAllServos();
        moveToPose(*s_serial, SLOT_SCAN1);
        showGreen(100);
    }
    else if (command == "POS2") {
        Serial.println("[指令] 摆到位姿2");
        stopGait();
        s_pendingGait = GAIT_NONE;
        strCmdLoadAllServos();
        moveToPose(*s_serial, SLOT_SCAN2);
        showGreen(100);
    }
    else if (command == "RESET") {
        Serial.println("[指令] 回正");
        stopGait();
        s_pendingGait = GAIT_NONE;
        strCmdLoadAllServos();
        resetToCenter(*s_serial);
        showGreen(100);
    }

    // ======================== 运动指令（仅设标志，loop 中执行） ========================
    else if (command.startsWith("VERT_")) {
        stopGait(); delay(30);
        parseGaitParams(command.substring(5), GAIT_VERT);
        s_pendingGait = GAIT_VERT;
        s_ble->sendDataToClient("VERT amp=" + String(s_gaitParams.ampVert,1)
            + " speed=" + String(s_gaitParams.speedVert,1)
            + " lambda=" + String(s_gaitParams.lambdaVert,1)
            + " bias=" + String(s_gaitParams.bias)
            + " unload=" + String(s_gaitParams.otherUnload) + "\r\n");
    }
    else if (command.startsWith("HORIZ_")) {
        stopGait(); delay(30);
        parseGaitParams(command.substring(6), GAIT_HORIZ);
        s_pendingGait = GAIT_HORIZ;
        s_ble->sendDataToClient("HORIZ amp=" + String(s_gaitParams.ampHoriz,1)
            + " speed=" + String(s_gaitParams.speedHoriz,1)
            + " lambda=" + String(s_gaitParams.lambdaHoriz,1)
            + " bias=" + String(s_gaitParams.bias)
            + " unload=" + String(s_gaitParams.otherUnload) + "\r\n");
    }
    else if (command.startsWith("DUAL_")) {
        stopGait(); delay(30);
        parseGaitParams(command.substring(5), GAIT_DUAL);
        s_pendingGait = GAIT_DUAL;
        s_ble->sendDataToClient("DUAL ampV=" + String(s_gaitParams.ampVert,1)
            + " ampH=" + String(s_gaitParams.ampHoriz,1)
            + " spV=" + String(s_gaitParams.speedVert,1)
            + " spH=" + String(s_gaitParams.speedHoriz,1)
            + " lambdaV=" + String(s_gaitParams.lambdaVert,1)
            + " lambdaH=" + String(s_gaitParams.lambdaHoriz,1)
            + " phase=" + String(s_gaitParams.phaseDiff,1) + "\r\n");
    }
    else if (command == "STOP") {
        stopGait();
        s_pendingGait = GAIT_NONE;
        showGreen(100);
        s_ble->sendDataToClient("Gait stopped.\r\n");
    }

    // --- 参数设置指令 ---
    else if (command.startsWith("POSTIME_")) {
        uint16_t t = (uint16_t)command.substring(8).toInt();
        if (t > 0) {
            g_defaultMoveTime = t;
            s_ble->sendDataToClient("Pose move time set to " + String(t) + " ms\r\n");
        }
    }
    else if (command.startsWith("SINETIME_")) {
        uint16_t t = (uint16_t)command.substring(9).toInt();
        if (t > 0) {
            setSineMoveTime(t);
            s_ble->sendDataToClient("Sine move time set to " + String(t) + " ms\r\n");
        }
    }

    // --- 参数查询 ---
    else if (command == "CMD_INFO") {
        String info = "=== Movement Info ===\r\n";
        info += "Servos: " + String(getServoCount()) + "\r\n";
        info += "Center source: slot " + String(getCenterSourceSlot())
                + " (" + String(getSlotName(getCenterSourceSlot())) + ")\r\n";
        info += "Lambda: " + String(getLambda(), 1) + " joints\r\n";
        info += "Max Amp: " + String(getMaxAmplitudeDeg(), 1) + " deg\r\n";
        info += "Max Speed: " + String(getMaxWaveSpeed(), 1) + " joints/s\r\n";
        info += "Sine Move Time: " + String(getSineMoveTime()) + " ms\r\n";
        info += "Pose Move Time: " + String(g_defaultMoveTime) + " ms\r\n";
        s_ble->sendDataToClient(info);
        showGreen(100);
    }

    // --- 帮助 ---
    else if (command == "HELP") {
        String help = "=== Commands ===\r\n";
        help += "Setup: SCAN,SCAN2,CALIBRATE,UNLOAD,LOAD,LIST,CLEAR_EEPROM\r\n";
        help += "Center: CENTER_CALIB,CENTER_SCAN1,CENTER_SCAN2\r\n";
        help += "Params: POSTIME_ms  SINETIME_ms\r\n";
        help += "Pose: POS1,POS2,RESET\r\n";
        help += "Gait: VERT_amp_speed_[lambda]_[bias]_[unload]\r\n";
        help += "      HORIZ_amp_speed_[lambda]_[bias]_[unload]\r\n";
        help += "      DUAL_ampV_ampH_spV_spH_[lambdaV]_[lambdaH]_[phase]\r\n";
        help += "      STOP  CMD_INFO  HELP\r\n";
        s_ble->sendDataToClient(help);
        showGreen(100);
    }

    else {
        Serial.println("[指令] 未知: " + command);
        s_ble->sendDataToClient("Unknown. Send HELP for command list.\r\n");
    }
}
