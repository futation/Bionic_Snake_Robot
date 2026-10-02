/**
 * ActionGroup.cpp - 动作组模式操作实现
 */

#include "ActionGroup.h"
#include "AppManager.h"
#include "../servo/SerialServoMove.h"
#include "../servo/SerialServoControl.h"
#include "../motion/Calibration.h"
#include "../servo/ServoRegistry.h"
#include "../motion/movement.h"
#include "../hal/RBG.h"
// ======================== 模式切换 ========================

void enterActionGroupMode() {
    if (getSystemMode() != MODE_NORMAL) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }
    setSystemMode(MODE_ACTION_GROUP);
    Serial.println("[模式] 进入动作组模式");

    // 停止当前步态运动
    stopGait();

    bpSendFeedback(BP_FB_ENTER_ACTION_OK);
}

void exitActionGroupMode() {
    if (getSystemMode() != MODE_ACTION_GROUP) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }
    setSystemMode(MODE_NORMAL);
    Serial.println("[模式] 退出动作组模式");
    bpSendFeedback(BP_FB_EXIT_ACTION_OK);
}

// ======================== 动作组操作 ========================

void powerOffServos(HardwareSerial &serial) {
    if (getSystemMode() != MODE_ACTION_GROUP) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }

    Serial.println("[动作组] 舵机掉电");
    LobotSerialServoSetLoad(serial, LOBOT_SERVO_ID_ALL, 0);
    showRed(100);
    bpSendFeedback(BP_FB_POWER_OFF_OK);
}

void scanServoPositions(HardwareSerial &serial) {
    if (getSystemMode() != MODE_ACTION_GROUP) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }

    Serial.println("[动作组] 扫描舵机位置...");

    // 1. 上电
    LobotSerialServoSetLoad(serial, LOBOT_SERVO_ID_ALL, 1);
    showGreen(100);
    // 2. 等待稳定
    delay(100);

    // 3. 扫描总线上所有舵机
    uint8_t  ids[MAX_REGISTERED_SERVOS];
    int16_t  positions[MAX_REGISTERED_SERVOS];
    uint8_t  count = scanServoBus(serial, ids, positions);

    // 4. 逐 ID 读取实时位置（scanServoBus 已读取位置，此处直接使用）
    //    scanServoBus 返回的 positions 即为实时位置
    Serial.printf("[动作组] 扫描完成，共 %d 个舵机\n", count);

    // 5. 上报扫描结果 (0x0D)
    bpSendScanResult(count, ids, positions);
}

void executeActionGroup(HardwareSerial &serial, const uint8_t* data, uint8_t count) {
    if (getSystemMode() != MODE_ACTION_GROUP) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }

    if (count == 0 || count > MAX_REGISTERED_SERVOS) {
        bpSendFeedbackFail(BP_ERR_INVALID_PARAM);
        return;
    }

    // 解析 Payload: [ID(1) Pos_L(1) Pos_H(1) Time_L(1) Time_H(1)] × count
    GroupServoAction actions[MAX_REGISTERED_SERVOS];

    for (uint8_t i = 0; i < count; i++) {
        const uint8_t* entry = data + i * 5;
        actions[i].servoId   = entry[0];
        actions[i].position  = (int16_t)((uint16_t)entry[1] | ((uint16_t)entry[2] << 8));
        actions[i].moveTime  = (uint16_t)((uint16_t)entry[3] | ((uint16_t)entry[4] << 8));
    }

    // 打印动作组信息
    Serial.printf("[动作组] 执行 %d 个舵机:\n", count);
    // for (uint8_t i = 0; i < count; i++) {
    //     Serial.printf("  ID:%d Pos:%d Time:%dms\n",
    //                   actions[i].servoId, actions[i].position, actions[i].moveTime);
    // }

    // 使用 individual 方式同步发送（所有舵机同时开始运动）
    moveAllImmediateIndividual(actions, count);

    // bpSendFeedback(BP_FB_GENERAL_OK);
}
