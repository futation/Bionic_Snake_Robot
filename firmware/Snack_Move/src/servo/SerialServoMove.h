#ifndef SERIALSERVOMOVE_H
#define SERIALSERVOMOVE_H

#include "SerialServoInclude.h"
#include <Arduino.h>

/*========================================================================
 * SerialServoMove - 舵机运动控制模块
 *
 * 提供舵机的各种运动控制功能：立即移动、预设移动、启动、停止、
 * 位置读取、移动参数读取、转动距离读取、顺序运动等。
 * 参考文档：《02+总线舵机通信协议.pdf》指令1,2,7,8,11,12,28,29,31,48
 *========================================================================*/

// ======================== 写指令（无返回值） ========================

/**
 * @brief  立即控制舵机转动到指定位置（指令1）
 * @param  SerialX  串口对象引用
 * @param  id       舵机ID
 * @param  position 目标位置 (0~1000)
 * @param  time     运动时间 (0~30000 ms)
 */
void LobotSerialServoMove(HardwareSerial &SerialX, uint8_t id, int16_t position, uint16_t time);

/**
 * @brief  预设舵机位置，等待START指令执行（指令7）
 *
 *         由于舵机硬件不支持指令8（读取预设参数），预设位置和时间
 *         由软件层通过 SingleServoManager 内部成员维护。
 *
 * @param  SerialX  串口对象引用
 * @param  id       舵机ID
 * @param  position 目标位置 (0~1000)
 * @param  time     运动时间 (0~30000 ms)
 */
void LobotSerialServoMoveTimeWait(HardwareSerial &SerialX, uint8_t id, int16_t position, uint16_t time);

/**
 * @brief  启动预设的位置移动（指令11）
 * @param  SerialX 串口对象引用
 * @param  id      舵机ID
 */
void LobotSerialServoMoveStart(HardwareSerial &SerialX, uint8_t id);

/**
 * @brief  停止舵机运动（指令12）
 * @param  SerialX 串口对象引用
 * @param  id      舵机ID
 */
void LobotSerialServoStopMove(HardwareSerial &SerialX, uint8_t id);

// ======================== 读指令（返回状态值） ========================

/**
 * @brief  读取舵机当前位置（指令28）
 * @param  SerialX 串口对象引用
 * @param  id      舵机ID
 * @return >=0: 当前位置 (0~1000)
 *         LOBOT_SERVO_ERR_TIMEOUT (-1): 超时
 *         LOBOT_SERVO_ERR_CHECKSUM (-2): 校验和错误
 */
int LobotSerialServoReadPosition(HardwareSerial &SerialX, uint8_t id);

/**
 * @brief  读取立即移动的参数（指令2）
 * @param  SerialX  串口对象引用
 * @param  id       舵机ID
 * @param  position 输出：预设位置
 * @param  time     输出：预设时间
 * @return LOBOT_SERVO_OK (1): 成功
 *         LOBOT_SERVO_ERR_TIMEOUT (-1): 超时
 *         LOBOT_SERVO_ERR_CHECKSUM (-2): 校验和错误
 */
int LobotSerialServoReadMoveTime(HardwareSerial &SerialX, uint8_t id, int16_t *position, uint16_t *time);

/*
  读取预设舵机移动参数（指令8）
  经过测试发现舵机对该指令没有返回，猜测该指令只在特殊的舵机才生效
  但蛇型机器人上的舵机都是不支持的。
  预设参数改为由软件层通过 SingleServoManager 内部成员维护，
  在调用 LobotSerialServoMoveTimeWait() 时同步更新。
  等到了驱动层再做封装处理。

// int LobotSerialServoReadMoveTimeWait(HardwareSerial &SerialX, uint8_t id, int16_t *position, uint16_t *time);
*/

// ======================== 工作模式 ========================

/**
 * @brief  设置舵机工作模式（指令29）
 * @param  id    舵机ID
 * @param  mode  模式：0=位置伺服模式, 1=电机调速模式
 * @param  speed 电机模式下的转速 (-1000~1000 占空比，或 -50~50 转速)
 * @note   位置模式下 speed 参数无意义但需传入（建议传0）
 */
void LobotSerialServoSetMode(HardwareSerial &SerialX, uint8_t id, uint8_t mode, int16_t speed);

// ======================== 加载/卸载 ========================

/**
 * @brief  设置电机加载/卸载状态（指令31）
 * @param  id   舵机ID
 * @param  load 0=卸载（无力矩输出）, 1=加载（有力矩输出）
 */
void LobotSerialServoSetLoad(HardwareSerial &SerialX, uint8_t id, uint8_t load);


/*  阅读总线舵机协议手册，对该指令的描述显得模棱两可
    经过测试发现舵机对该指令没有返回，猜测该指令只在特殊的舵机才生效
    但蛇型机器人上的舵机都是不支持的
    所以,这里我们不做编译
*/

// /**
//  * @brief  读取舵机转动距离（指令48）
//  * @param  SerialX 串口对象引用
//  * @param  id      舵机ID
//  * @return >=0: 转动距离（脉冲数，uint32_t 强制转为 int，注意范围）
//  *         LOBOT_SERVO_ERR_TIMEOUT (-1): 超时
//  *         LOBOT_SERVO_ERR_CHECKSUM (-2): 校验和错误
//  * @note   返回值为32位无符号数，用int接收时注意溢出
//  *         建议使用 LobotSerialServoReadDistanceU32() 获取完整32位值
//  */
// int LobotSerialServoReadDistance(HardwareSerial &SerialX, uint8_t id);


#endif // SERIALSERVOMOVE_H
