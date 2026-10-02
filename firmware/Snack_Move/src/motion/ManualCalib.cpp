/**
 * ManualCalib.cpp - 手动校准 & ID 调试模式实现
 */

#include "ManualCalib.h"
#include "Calibration.h"      // scanServoBus()
#include "../servo/SerialServoMove.h"
#include "../servo/SerialServoID.h"

// ======================== 模式切换 ========================

void enterManualCalibMode(HardwareSerial &serial) {
    if (getSystemMode() != MODE_NORMAL) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }
    setSystemMode(MODE_MANUAL_CALIB);
    Serial.println("[模式] 进入手动校准模式");
    //模式切换成功反馈
    bpSendFeedback(BP_FB_ENTER_MANUAL_OK);
    // 回中(所有舵机回到 EEPROM 中保存的位置)
    returnAllToCenter(serial);

    // 读取所有舵机信息并返回
    readAllAndRespond(serial);
}

void exitManualCalibMode() {
    if (getSystemMode() != MODE_MANUAL_CALIB) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }
    setSystemMode(MODE_NORMAL);
    Serial.println("[模式] 退出手动校准模式");
    bpSendFeedback(BP_FB_EXIT_MANUAL_OK);
}

// ======================== 手动校准操作 ========================

void handleManualSetServo(HardwareSerial &serial, uint8_t id,
                          int16_t position, int8_t offset) {
    if (getSystemMode() != MODE_MANUAL_CALIB) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }

    if (position < 0 || position > 1000 || offset < -125 || offset > 125) {
        bpSendFeedbackFail(BP_ERR_INVALID_PARAM);
        return;
    }

    // 移动舵机
    LobotSerialServoMove(serial, id, position, 100);
    delay(50);

    // 临时调整角度偏差（不保存到舵机 EEPROM）
    LobotSerialServoAdjustOffset(serial, id, offset);
    delay(20);

    Serial.printf("[手动校准] ID:%d  Pos:%d  Offset:%d (临时)\n", id, position, offset);
    // bpSendFeedback(BP_FB_GENERAL_OK);
}

void handleManualSave(HardwareSerial &serial, uint8_t id,
                      int16_t position, int8_t offset) {
    if (getSystemMode() != MODE_MANUAL_CALIB) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }

    if (position < 0 || position > 1000 || offset < -125 || offset > 125) {
        bpSendFeedbackFail(BP_ERR_INVALID_PARAM);
        return;
    }

    // 移动舵机
    LobotSerialServoMove(serial, id, position, 100);
    delay(50);

    // 调整偏差 + 保存到舵机 EEPROM
    LobotSerialServoAdjustOffset(serial, id, offset);
    delay(20);
    LobotSerialServoSaveOffset(serial, id);
    delay(30);

    // 更新校准槽中的 Position
    ServoRegistry* reg = getRegistry(SLOT_CALIB);
    if (reg) {
        // 查找或添加该 ID 的记录
        bool found = false;
        for (uint8_t i = 0; i < reg->count; i++) {
            if (reg->servos[i].id == id) {
                reg->servos[i].position = position;
                found = true;
                break;
            }
        }
        if (!found && reg->count < MAX_REGISTERED_SERVOS) {
            reg->servos[reg->count].id = id;
            reg->servos[reg->count].position = position;
            reg->count++;
        }
        saveRegistry(SLOT_CALIB, nullptr);
    }

    Serial.printf("[手动校准] ID:%d  Pos:%d  Offset:%d (已永久保存)\n", id, position, offset);
    bpSendFeedback(BP_FB_SAVE_PARAMS_OK);
}

// ======================== ID 调试操作 ========================

void handleChangeID(HardwareSerial &serial, uint8_t oldID, uint8_t newID) {
    // ID 修改在校准模式下进行
    if (getSystemMode() != MODE_MANUAL_CALIB) {
        bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
        return;
    }

    if (oldID > 253 || newID > 253) {
        bpSendFeedbackFail(BP_ERR_INVALID_PARAM);
        return;
    }

    LobotSerialServoSetID(serial, oldID, newID);
    delay(50);

    // 更新校准槽 EEPROM 中的记录
    ServoRegistry* reg = getRegistry(SLOT_CALIB);
    if (reg) {
        for (uint8_t i = 0; i < reg->count; i++) {
            if (reg->servos[i].id == oldID) {
                reg->servos[i].id = newID;
                saveRegistry(SLOT_CALIB, nullptr);
                break;
            }
        }
    }

    Serial.printf("[ID修改] ID %d → %d\n", oldID, newID);
    bpSendFeedback(BP_FB_CHANGE_ID_OK);
}

// ======================== 通用操作 ========================

void returnAllToCenter(HardwareSerial &serial) {
    Serial.println("[回中] 所有舵机回到 EEROM/500 脉冲");

    // 从校准槽读取已注册的舵机
    ServoRegistry* reg = getRegistry(SLOT_CALIB);
    uint8_t count = (reg && reg->count > 0) ? reg->count : 0;

    if (count > 0) {
        for (uint8_t i = 0; i < count; i++) {
            LobotSerialServoMove(serial, reg->servos[i].id,
                                 reg->servos[i].position, 500);
            delay(30);
        }
    } else {
        // 没有校准数据 → 扫描总线 → 全部移到 500
        uint8_t  ids[MAX_REGISTERED_SERVOS];
        int16_t  positions[MAX_REGISTERED_SERVOS];
        uint8_t  cnt = scanServoBus(serial, ids, positions);
        for (uint8_t i = 0; i < cnt; i++) {
            LobotSerialServoMove(serial, ids[i], 500, 500);
            delay(30);
        }
    }

    delay(600);  // 等待所有舵机到位
}

void readAllAndRespond(HardwareSerial &serial) {
    uint8_t  ids[MAX_REGISTERED_SERVOS];
    int16_t  positions[MAX_REGISTERED_SERVOS];
    int8_t   offsets[MAX_REGISTERED_SERVOS];

    uint8_t count = scanServoBus(serial, ids, positions);

    // 读取每个舵机的角度偏差
    for (uint8_t i = 0; i < count; i++) {
        int off = LobotSerialServoReadOffset(serial, ids[i]);
        offsets[i] = (off >= -125 && off <= 125) ? (int8_t)off : 0;
        delay(10);
    }

    Serial.printf("[读取全部] 共 %d 个舵机\n", count);
    bpSendReportAllServos(count, ids, positions, offsets);
}
