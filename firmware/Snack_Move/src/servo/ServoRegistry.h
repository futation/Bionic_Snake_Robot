#ifndef SERVO_REGISTRY_H
#define SERVO_REGISTRY_H

#include <Arduino.h>
#include "../comm/Bluetooth.h"
/*========================================================================
 * ServoRegistry - 舵机注册表 EEPROM 多槽位存储模块
 *
 * 三个独立槽位，互不覆盖：
 *   槽0 (SLOT_CALIB) — 校准数据（校准扫描后保存）
 *   槽1 (SLOT_SCAN1) — 静态动作1（用户自定义位姿）
 *   槽2 (SLOT_SCAN2) — 静态动作2（用户自定义位姿）
 *
 * EEPROM 布局：3 槽 × 128 字节 = 384 字节 << 4KB
 *========================================================================*/

// ======================== 常量 ========================

#define MAX_REGISTERED_SERVOS  21      // 最大舵机数 (ID 0~20)
#define EEPROM_MAGIC           0xA5    // 魔数：标记数据有效
#define EEPROM_SLOT_SIZE       128     // 每槽字节数（含余量）

// 槽位编号
#define SLOT_CALIB  0   // 校准数据
#define SLOT_SCAN1  1   // 静态动作 1
#define SLOT_SCAN2  2   // 静态动作 2
#define SLOT_COUNT  3   // 槽位总数

// ======================== 数据结构 ========================

/**
 * @struct ServoRecord
 * @brief  单条舵机记录（存入 EEPROM）
 */
struct ServoRecord {
    uint8_t  id;        // 舵机ID
    int16_t  position;  // 当前位置 (0~1000)
};

/**
 * @struct ServoRegistry
 * @brief  舵机注册表（内存中维护，整体写入 EEPROM）
 */
struct ServoRegistry {
    uint8_t     magic;                       // 魔数 (0xA5 = 有效)
    uint8_t     count;                       // 已注册舵机数量
    ServoRecord servos[MAX_REGISTERED_SERVOS]; // 记录数组
};

// ======================== 全局注册表实例 ========================

extern ServoRegistry calibRegistry;   // 槽0：校准数据
extern ServoRegistry scan1Registry;   // 槽1：静态动作1
extern ServoRegistry scan2Registry;   // 槽2：静态动作2

// ======================== 辅助函数 ========================

/** @brief 根据槽位编号获取对应注册表指针，非法槽位返回 nullptr */
ServoRegistry* getRegistry(uint8_t slot);

/** @brief 获取槽位名称字符串 */
const char* getSlotName(uint8_t slot);

// ======================== EEPROM API（带槽位参数） ========================

/** @brief 初始化 EEPROM，必须在使用其他函数前调用 */
void initEEPROM();

/** @brief 将指定槽位写入 EEPROM，返回 true 成功 */
bool saveRegistry(uint8_t slot, Bluetooth* ble);

/** @brief 从 EEPROM 加载指定槽位，返回 true=有有效数据 */
bool loadRegistry(uint8_t slot);

/** @brief 清除指定槽位（内存 + EEPROM） */
void clearRegistry(uint8_t slot);

/** @brief 一键清除所有槽位 */
void clearAllEEPROM();

/** @brief 串口打印指定槽位的舵机列表 */
void printRegistry(uint8_t slot);


// ======================== 角度偏差函数声明 ========================

#include "SerialServoInclude.h"

/** @brief 立即调整角度偏差（指令17，不保存） */
void LobotSerialServoAdjustOffset(HardwareSerial &SerialX, uint8_t id, int8_t offset);

/** @brief 保存角度偏差到舵机（指令18，掉电不丢失） */
void LobotSerialServoSaveOffset(HardwareSerial &SerialX, uint8_t id);

/** @brief 读取舵机角度偏差（指令19） */
int LobotSerialServoReadOffset(HardwareSerial &SerialX, uint8_t id);

#endif

