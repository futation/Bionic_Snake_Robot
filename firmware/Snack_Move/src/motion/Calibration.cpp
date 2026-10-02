/**
 * Calibration.cpp - 舵机校准扫描模块实现
 */

#include "Calibration.h"
#include "../servo/SerialServoMove.h"
#include "../servo/SerialServoID.h"
#include "../comm/Bluetooth.h"
#include "movement.h"

// ======================================================================
//  scanServoBus — 扫描总线，收集 (ID, Position)
// ======================================================================

uint8_t scanServoBus(HardwareSerial &serial, uint8_t *idsOut, int16_t *positionsOut) {
    uint8_t count = 0;

    Serial.println("[扫描] 遍历 ID 0~20...");
    for (uint8_t id = 0; id <= 20; id++) {
        int pos = LobotSerialServoReadPosition(serial, id);
        if (pos < 0) continue;   // 无响应 (超时/校验错)

        if (count < MAX_REGISTERED_SERVOS) {
            idsOut[count] = id;
            positionsOut[count] = pos;
            count++;
        }
        Serial.printf("[扫描] ID:%d  Pos:%d\n", id, pos);
        delay(20);  // 避免总线拥堵
    }

    Serial.printf("[扫描] 完成，共发现 %d 个舵机\n", count);
    return count;
}

// ======================================================================
//  renumberServoIDs — ID 重排
// ======================================================================

uint8_t renumberServoIDs(HardwareSerial &serial, const uint8_t *oldIDs, uint8_t count) {
    // 检查是否已经连续 (0,1,2,...,N-1)
    bool alreadySequential = true;
    for (uint8_t i = 0; i < count; i++) {
        if (oldIDs[i] != i) {
            alreadySequential = false;
            break;
        }
    }

    if (alreadySequential) {
        Serial.println("[ID重排] ID 已连续 (0~" + String(count - 1) + ")，跳过");
        return 0;
    }

    Serial.println("[ID重排] 开始重命名...");
    uint8_t renamed = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (oldIDs[i] != i) {
            Serial.printf("[ID重排] ID %d → %d\n", oldIDs[i], i);
            LobotSerialServoSetID(serial, oldIDs[i], i);
            renamed++;
            delay(20);  // 给舵机处理时间
        }
    }
    Serial.printf("[ID重排] 完成，重命名 %d 个舵机\n", renamed);
    return renamed;
}

// ======================================================================
//  calibrateSingleOffset — 校准单个舵机的角度偏差
// ======================================================================

bool calibrateSingleOffset(HardwareSerial &serial,  Bluetooth* ble , uint8_t id,
                           int16_t physicalCenter, int16_t *outPosition) {

    // 1. 计算需要的偏差
    int16_t offsetNeeded = physicalCenter - CALIB_CENTER_POS;
    int8_t  offsetClamped;
    int16_t adjustedPos = CALIB_CENTER_POS;   // 默认不补偿

    // 钳位偏差到 ±125 范围
    bool withinLimit = true;
    if (offsetNeeded > CALIB_OFFSET_LIMIT) {
        offsetClamped = CALIB_OFFSET_LIMIT;
        adjustedPos = CALIB_CENTER_POS + (offsetNeeded - offsetClamped);
        withinLimit = false;
    } else if (offsetNeeded < -CALIB_OFFSET_LIMIT) {
        offsetClamped = -CALIB_OFFSET_LIMIT;
        adjustedPos = CALIB_CENTER_POS + (offsetNeeded - offsetClamped);
        withinLimit = false;
    } else {
        offsetClamped = (int8_t)offsetNeeded;
    }

    // 2. 移动舵机到补偿后的位置(不用补偿则是500)
    LobotSerialServoMove(serial, id, adjustedPos, CALIB_MOVE_TIME);
    delay(CALIB_MOVE_TIME + 100);

    // 3. 设置并保存角度偏差
    LobotSerialServoAdjustOffset(serial, id, offsetClamped);
    delay(20);
    LobotSerialServoSaveOffset(serial, id);
    delay(20);

    // 4. 输出补偿后位置
    if (outPosition != nullptr) *outPosition = adjustedPos;

    // 5. 蓝牙回传结果
    if (ble) {
        String msg = "[校准] ID:" + String(id)
                     + " physical:" + String(physicalCenter)
                     + " offset:" + String(offsetClamped)
                     + " adjPos:" + String(adjustedPos);
        if (!withinLimit) {
            msg += " (LIMIT! needed:" + String(offsetNeeded) + ")";
        }
        msg += "\r\n";
        ble->sendDataToClient(msg);
    }
    if (!withinLimit) {
        Serial.printf("[校准] ID:%d 物理中心:%d 需要offset:%d 超出极限±%d，已钳位到:%d\n",
                      id, physicalCenter, offsetNeeded, CALIB_OFFSET_LIMIT, offsetClamped);
    }

    return withinLimit;
}

// ======================================================================
//  calibrateAllServos — 完整校准流程
// ======================================================================

void calibrateAllServos(HardwareSerial &serial, Bluetooth *ble) {
    Serial.println("\n========== 校准扫描开始 ==========");

    // ===== Step 1: 确保上电 =====
    LobotSerialServoSetLoad(serial, LOBOT_SERVO_ID_ALL, 1);
    delay(200);
    Serial.println("[校准] 舵机已上电");

    // ===== Step 2: 扫描所有舵机 =====
    uint8_t  oldIDs[MAX_REGISTERED_SERVOS];
    int16_t  positions[MAX_REGISTERED_SERVOS];
    uint8_t  servoCount = scanServoBus(serial, oldIDs, positions);

    if (servoCount == 0) {
        Serial.println("[校准] 错误：未检测到任何舵机！");
        if (ble) ble->sendDataToClient("Calibration FAILED: No servos found.\r\n");
        return;
    }

    if (ble) {
        ble->sendDataToClient("Found " + String(servoCount) + " servo(s). Renumbering...\r\n");
    }

    // ===== Step 3: ID 重排 =====
    renumberServoIDs(serial, oldIDs, servoCount);

    // ===== Step 4: 分两轮校准角度偏差 =====
    Serial.println("[校准] 开始角度偏差校准...");
    int16_t compensatedPos[MAX_REGISTERED_SERVOS];
    for (uint8_t i = 0; i < servoCount; i++) {
        compensatedPos[i] = CALIB_CENTER_POS;  // 默认值
    }
    // Round 1: 奇数 ID (1, 3, 5, ...)
    Serial.println("[校准] -- 第1轮：奇数ID --");
    for (uint8_t i = 1; i < servoCount; i += 2) {
        calibrateSingleOffset(serial, ble, i, positions[i], &compensatedPos[i]);
    }

    // Round 2: 偶数 ID (0, 2, 4, ...)
    Serial.println("[校准] -- 第2轮：偶数ID --");
    for (uint8_t i = 0; i < servoCount; i += 2) {
        calibrateSingleOffset(serial, ble, i, positions[i], &compensatedPos[i]);
    }


    // ===== Step 5: 保存到 EEPROM 校准槽 =====
    calibRegistry.count = servoCount;
    for (uint8_t i = 0; i < servoCount; i++) {
        calibRegistry.servos[i].id = i;
        calibRegistry.servos[i].position = compensatedPos[i];
    }
    saveRegistry(SLOT_CALIB,ble);

    // ===== Step 5.5: 同步到运动模块 =====
    loadRegistry(SLOT_CALIB);
    reloadCenterFromSlot(SLOT_CALIB, ble);

    Serial.println("========== 校准扫描完成 ==========\n");
    if (ble) {
        ble->sendDataToClient("Calibration complete. "
                              + String(servoCount) + " servos calibrated.\r\n");
    }

    // Step 6: 验证读取
    Serial.println("[校准] 验证角度偏差...");
    for (uint8_t i = 0; i < servoCount; i++) {
        delay(30);  // 等待舵机 EEPROM 写入完成
        int readBack = LobotSerialServoReadOffset(serial, i);
        // readBack 返回的是 int8_t 值
        Serial.printf("[验证] ID:%d offset=%d\n", i, readBack);
        if (ble) {
            ble->sendDataToClient("[验证] ID:" + String(i) 
                + " offset:" + String(readBack) + "\r\n");
        }
    }
}
