#ifndef SCANSLOTS_H
#define SCANSLOTS_H

#include <Arduino.h>
#include "../servo/ServoRegistry.h"
#include "../servo/SerialServoMove.h"
/*========================================================================
 * ScanSlots - 普通扫描槽位模块
 *
 * 提供 SCAN1 / SCAN2 指令的扫描与保存功能。
 * 扫描结果存入 EEPROM 槽位 1 或 2，不受校准槽 (槽0) 影响。
 *========================================================================*/

/**
 * @brief 扫描总线并保存到指定槽位
 *
 * 遍历 ID 0~20，收集所有在线舵机的 (ID, Position)，
 * 保存到 EEPROM 的指定槽位。
 *
 * @param serial  舵机总线串口
 * @param slot    目标槽位 (SLOT_SCAN1 或 SLOT_SCAN2)
 * @param ble     蓝牙对象指针（用于回传结果），可为 nullptr
 */
void scanAndSaveToSlot(HardwareSerial &serial, uint8_t slot, class Bluetooth *ble);

#endif
