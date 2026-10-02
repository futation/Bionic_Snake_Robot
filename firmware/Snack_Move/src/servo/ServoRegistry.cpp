/**
 * ServoRegistry.cpp - 舵机注册表 EEPROM 多槽位存储模块
 *
 * 功能：
 *   - 三个独立槽位：校准(0) / 静态动作1(1) / 静态动作2(2)
 *   - 每个槽位独立魔数验证，互不覆盖
 *   - 支持保存、加载、清除、打印
 */

#include "ServoRegistry.h"
#include <EEPROM.h>

// ======================== 全局注册表实例 ========================
ServoRegistry calibRegistry;   // 槽0
ServoRegistry scan1Registry;   // 槽1
ServoRegistry scan2Registry;   // 槽2

// ======================== 辅助函数 ========================

ServoRegistry* getRegistry(uint8_t slot) {
    switch (slot) {
        case SLOT_CALIB: return &calibRegistry;
        case SLOT_SCAN1: return &scan1Registry;
        case SLOT_SCAN2: return &scan2Registry;
        default:         return nullptr;
    }
}

const char* getSlotName(uint8_t slot) {
    switch (slot) {
        case SLOT_CALIB: return "校准";
        case SLOT_SCAN1: return "扫描1";
        case SLOT_SCAN2: return "扫描2";
        default:         return "未知";
    }
}

// ======================== EEPROM 核心实现 ========================

void initEEPROM() {
    size_t totalSize = EEPROM_SLOT_SIZE * SLOT_COUNT;
    EEPROM.begin(totalSize);
    Serial.println("[EEPROM] 初始化完成，总大小: " + String(totalSize)
                   + " 字节 (3槽 × " + String(EEPROM_SLOT_SIZE) + ")");
}

bool saveRegistry(uint8_t slot, Bluetooth* ble) {
    ServoRegistry* reg = getRegistry(slot);
    if (!reg) return false;

    reg->magic = EEPROM_MAGIC;
    int addr = slot * EEPROM_SLOT_SIZE;
    EEPROM.put(addr, *reg);
    bool ok = EEPROM.commit();

    if (ok) {
        Serial.println("[EEPROM] 槽" + String(slot) + "(" + getSlotName(slot)
                       + ") 保存成功 (" + String(reg->count) + " 个舵机)");
        if (ble) {
            ble->sendDataToClient("Saved " + String(reg->count) + " servo(s) to slot "
                                   + String(slot) + " (" + getSlotName(slot) + ").\r\n");
        }
    } else {
        Serial.println("[EEPROM] 槽" + String(slot) + "(" + getSlotName(slot)
                       + ") 保存失败!");
        if (ble) {
            ble->sendDataToClient("ERROR: Failed to save slot " + String(slot) + " (" + getSlotName(slot) + ").\r\n");
        }
    }
    return ok;
}

bool loadRegistry(uint8_t slot) {
    ServoRegistry* reg = getRegistry(slot);
    if (!reg) return false;

    ServoRegistry temp;
    int addr = slot * EEPROM_SLOT_SIZE;
    EEPROM.get(addr, temp);

    if (temp.magic != EEPROM_MAGIC) {
        // 首次上电或数据损坏 — 清零内存中的注册表
        reg->magic = 0;
        reg->count = 0;
        Serial.println("[EEPROM] 槽" + String(slot) + "(" + getSlotName(slot)
                       + ") 无有效数据 (魔数不匹配)");
        return false;
    }

    // 有效数据 → 复制到全局注册表
    memcpy(reg, &temp, sizeof(ServoRegistry));
    Serial.println("[EEPROM] 槽" + String(slot) + "(" + getSlotName(slot)
                   + ") 加载成功 (" + String(reg->count) + " 个舵机)");
    return true;
}

void clearRegistry(uint8_t slot) {
    ServoRegistry* reg = getRegistry(slot);
    if (!reg) return;

    // 清零内存
    reg->magic = 0;
    reg->count = 0;
    memset(reg->servos, 0, sizeof(reg->servos));

    // 写入 EEPROM
    int addr = slot * EEPROM_SLOT_SIZE;
    EEPROM.put(addr, *reg);
    EEPROM.commit();

    Serial.println("[EEPROM] 槽" + String(slot) + "(" + getSlotName(slot) + ") 已清除");
}

void clearAllEEPROM() {
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        clearRegistry(i);
    }
    Serial.println("[EEPROM] 所有槽位已清除");
}

void printRegistry(uint8_t slot) {
    ServoRegistry* reg = getRegistry(slot);
    if (!reg) return;

    if (reg->count == 0) {
        Serial.println("[" + String(getSlotName(slot)) + "] 空");
        return;
    }

    Serial.println("========== [" + String(getSlotName(slot)) + "] "
                   + String(reg->count) + " 个舵机 ==========");
    for (uint8_t i = 0; i < reg->count; i++) {
        Serial.print("  ID: ");
        Serial.print(reg->servos[i].id);
        Serial.print("\tPosition: ");
        Serial.println(reg->servos[i].position);
    }
    Serial.println("==========================================");
}

// ======================================================================
//  角度偏差函数（舵机协议指令 17/18/19）
//  这些函数操作舵机内部寄存器，与 EEPROM 无关。
//  声明在 ServoRegistry.h 中以便外部调用。
// ======================================================================

void LobotSerialServoAdjustOffset(HardwareSerial &SerialX, uint8_t id, int8_t offset)
{
    byte buf[7];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 4;
    buf[4] = LOBOT_SERVO_ANGLE_OFFSET_ADJUST;    // Cmd=17
    buf[5] = (byte)offset;
    buf[6] = LobotCheckSum(buf);

    SerialX.write(buf, 7);
}

void LobotSerialServoSaveOffset(HardwareSerial &SerialX, uint8_t id)
{
    byte buf[6];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = LOBOT_SERVO_ANGLE_OFFSET_WRITE;     // Cmd=18
    buf[5] = LobotCheckSum(buf);

    SerialX.write(buf, 6);
}

int LobotSerialServoReadOffset(HardwareSerial &SerialX, uint8_t id)
{
    int count = 10000;
    byte buf[6];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = LOBOT_SERVO_ANGLE_OFFSET_READ;  // Cmd=19
    buf[5] = LobotCheckSum(buf);
    SerialX.write(buf, 6);

    while (SerialX.available())
        SerialX.read();

    while (!SerialX.available()) {
        if (--count <= 0) return LOBOT_SERVO_ERR_TIMEOUT;
    }

    int result = LobotSerialServoReceiveHandle(SerialX, buf);
    if (result > 0) {
        return (int8_t)GET_LOW_BYTE(buf[0]);
    } else if (result == LOBOT_SERVO_ERR_TIMEOUT) {
        return LOBOT_SERVO_ERR_TIMEOUT;
    } else {
        return LOBOT_SERVO_ERR_CHECKSUM;
    }
}


