#ifndef SERIALSERVOID_H
#define SERIALSERVOID_H

#include "SerialServoInclude.h"
#include <Arduino.h>

/*========================================================================
 * SerialServoID - 舵机ID管理模块
 *
 * 提供舵机ID的读取与写入功能。
 * 参考文档：《02+总线舵机通信协议.pdf》指令13、14
 *========================================================================*/

/**
 * @brief  写入新ID到舵机（掉电保存）
 * @param  SerialX 串口对象引用
 * @param  oldID   当前舵机ID（1~253）
 * @param  newID   新舵机ID（1~253）
 * @note   指令13 (SERVO_ID_WRITE)，写指令无返回值
 */
void LobotSerialServoSetID(HardwareSerial &SerialX, uint8_t oldID, uint8_t newID);

/**
 * @brief  读取舵机ID（广播模式，总线上只能连接一个舵机）
 * @param  SerialX 串口对象引用
 * @return >=0: 舵机ID (1~253)
 *         LOBOT_SERVO_ERR_TIMEOUT (-1): 通信超时
 *         LOBOT_SERVO_ERR_CHECKSUM (-2): 校验和错误
 * @note   指令14 (SERVO_ID_READ)，使用广播ID=254
 */
int LobotSerialServoReadID(HardwareSerial &SerialX);

#endif // SERIALSERVOID_H