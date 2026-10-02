/**
 * ScanSlots.cpp - 普通扫描槽位模块实现
 */

#include "ScanSlots.h"
#include "Calibration.h"       // 复用 scanServoBus()
#include "../comm/Bluetooth.h"

// ======================================================================
//  scanAndSaveToSlot — 扫描并保存到指定槽位
// ======================================================================

void scanAndSaveToSlot(HardwareSerial &serial, uint8_t slot, Bluetooth *ble) {
    if (slot != SLOT_SCAN1 && slot != SLOT_SCAN2) {
        Serial.println("[扫描槽] 错误：无效槽位 " + String(slot));
        return;
    }

    const char *slotName = (slot == SLOT_SCAN1) ? "SCAN1" : "SCAN2";
    Serial.println("\n========== " + String(slotName) + " 扫描开始 ==========");

    // 确保上电
    LobotSerialServoSetLoad(serial, LOBOT_SERVO_ID_ALL, 1);
    delay(200);

    // 扫描总线
    uint8_t  ids[MAX_REGISTERED_SERVOS];
    int16_t  positions[MAX_REGISTERED_SERVOS];
    uint8_t  count = scanServoBus(serial, ids, positions);

    if (count == 0) {
        Serial.println("[" + String(slotName) + "] 未检测到任何舵机！");
        if (ble) ble->sendDataToClient(String(slotName) + ": No servos found.\r\n");
        return;
    }

    // 填充注册表
    ServoRegistry *reg = getRegistry(slot);
    reg->count = count;
    for (uint8_t i = 0; i < count; i++) {
        reg->servos[i].id       = ids[i];
        reg->servos[i].position = positions[i];
    }

    // 保存到 EEPROM
    bool saved = saveRegistry(slot,ble);

    // 蓝牙回传
    if (ble) {
        String result = String(slotName) + ": Found " + String(count)
                        + " servo(s). ";
        result += saved ? "Saved to EEPROM.\r\n" : "EEPROM save FAILED!\r\n";

        // 附上每条记录
        for (uint8_t i = 0; i < count; i++) {
            result += "  ID:" + String(ids[i])
                      + " Pos:" + String(positions[i]) + "\r\n";
        }
        ble->sendDataToClient(result);
    }

    Serial.println("========== " + String(slotName) + " 扫描完成 ==========\n");
}
