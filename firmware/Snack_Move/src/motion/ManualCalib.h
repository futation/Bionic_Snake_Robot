#ifndef MANUALCALIB_H
#define MANUALCALIB_H

#include <Arduino.h>
#include "../servo/ServoRegistry.h"
#include "../comm/BinaryProtocol.h"
#include "../app/AppManager.h"       // SystemMode 枚举 + getSystemMode/setSystemMode

/*========================================================================
 * ManualCalib - 手动校准模式操作
 *
 * 仅包含校准相关业务函数，模式管理已移至 AppManager。
 *========================================================================*/

// --- 模式切换 ---

/** @brief 进入手动校准模式（回中→读取→返回上位机） */
void enterManualCalibMode(HardwareSerial &serial);

/** @brief 退出手动校准模式 */
void exitManualCalibMode();

// --- 手动校准操作 ---

/**
 * @brief 临时调整单个舵机（不保存到舵机/EEPROM）
 * @note  仅在 MODE_MANUAL_CALIB 下有效
 */
void handleManualSetServo(HardwareSerial &serial, uint8_t id,
                          int16_t position, int8_t offset);

/**
 * @brief 永久保存单个舵机的校准（保存 offset 到舵机 + 更新 EEPROM 校准槽）
 * @note  仅在 MODE_MANUAL_CALIB 下有效
 */
void handleManualSave(HardwareSerial &serial, uint8_t id,
                      int16_t position, int8_t offset);

// --- ID 修改操作 ---

/**
 * @brief 修改舵机 ID
 * @note  仅在校准模式（MODE_MANUAL_CALIB）下有效
 */
void handleChangeID(HardwareSerial &serial, uint8_t oldID, uint8_t newID);

// --- 通用操作 ---

/** @brief 所有舵机回到 500 脉冲位置 */
void returnAllToCenter(HardwareSerial &serial);

/** @brief 读取所有在线舵机信息并通过二进制协议返回 */
void readAllAndRespond(HardwareSerial &serial);

#endif
