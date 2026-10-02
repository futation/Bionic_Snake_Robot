#ifndef SERIALSERVOINCLUDE_H
#define SERIALSERVOINCLUDE_H

#include <Arduino.h>

/*========================================================================
 * 总线舵机控制库 - 协议底层定义与公共声明
 * 参考文档：《02+总线舵机通信协议.pdf》
 *
 * 通信格式：0x55 0x55 ID Length Cmd Data... Checksum
 *   - 帧头：连续两个 0x55
 *   - ID：舵机ID（1~253），254=广播
 *   - Length：数据长度 = Cmd(1) + Data(N) + Checksum(1)
 *   - Cmd：指令号
 *   - Data：指令参数（小端序）
 *   - Checksum：校验和 = ~(ID + Length + Cmd + Data...)
 *========================================================================*/

// ======================== 宏定义 ========================

// 字节序转换宏
#define GET_LOW_BYTE(A)  ((uint8_t)((A) & 0x00FF))
#define GET_HIGH_BYTE(A) ((uint8_t)(((A) >> 8) & 0x00FF))
#define BYTE_TO_HW(A, B) ((((uint16_t)(A)) << 8) | (uint8_t)(B))

// 广播ID：用于读取总线上单个舵机的ID（总线上只能连接一个舵机）
#define LOBOT_SERVO_ID_ALL  254

// 协议帧头
#define LOBOT_SERVO_FRAME_HEADER  0x55

// 返回值约定
#define LOBOT_SERVO_OK           1     // 操作成功
#define LOBOT_SERVO_ERR_TIMEOUT  -2048     // 通信超时
#define LOBOT_SERVO_ERR_CHECKSUM -2049    // 校验和错误

// ======================== 全部29条指令定义 ========================

// --- 运动控制 (指令1,2,7,8,11,12) ---
#define LOBOT_SERVO_MOVE_TIME_WRITE       1   // 写：立即转动到指定位置（时间可设）
#define LOBOT_SERVO_MOVE_TIME_READ        2   // 读：读取位置控制参数（位置+时间）
#define LOBOT_SERVO_MOVE_TIME_WAIT_WRITE  7   // 写：预设位置（等待START指令执行）
#define LOBOT_SERVO_MOVE_TIME_WAIT_READ   8   // 读：读取预设位置参数（位置+时间）
#define LOBOT_SERVO_MOVE_START            11  // 写：启动预设移动
#define LOBOT_SERVO_MOVE_STOP             12  // 写：停止当前运动

// --- ID 管理 (指令13,14) ---
#define LOBOT_SERVO_ID_WRITE              13  // 写：设置舵机ID（掉电保存）
#define LOBOT_SERVO_ID_READ               14  // 读：读取舵机ID（支持广播）

// --- 角度偏差 (指令17,18,19) ---
#define LOBOT_SERVO_ANGLE_OFFSET_ADJUST   17  // 写：立即调整角度偏差（不保存）
#define LOBOT_SERVO_ANGLE_OFFSET_WRITE    18  // 写：保存角度偏差（掉电保存）
#define LOBOT_SERVO_ANGLE_OFFSET_READ     19  // 读：读取角度偏差

// --- 角度限制 (指令20,21) ---
#define LOBOT_SERVO_ANGLE_LIMIT_WRITE     20  // 写：设置角度限制范围（掉电保存）
#define LOBOT_SERVO_ANGLE_LIMIT_READ      21  // 读：读取角度限制范围

// --- 电压限制 (指令22,23) ---
#define LOBOT_SERVO_VIN_LIMIT_WRITE       22  // 写：设置电压限制范围（掉电保存）
#define LOBOT_SERVO_VIN_LIMIT_READ        23  // 读：读取电压限制范围

// --- 温度 (指令24,25,26) ---
#define LOBOT_SERVO_TEMP_MAX_LIMIT_WRITE  24  // 写：设置最高温度限制（掉电保存）
#define LOBOT_SERVO_TEMP_MAX_LIMIT_READ   25  // 读：读取最高温度限制
#define LOBOT_SERVO_TEMP_READ             26  // 读：读取当前温度

// --- 实时参数 (指令27,28) ---
#define LOBOT_SERVO_VIN_READ              27  // 读：读取当前输入电压 (mV)
#define LOBOT_SERVO_POS_READ              28  // 读：读取当前位置 (0-1000)

// --- 模式控制 (指令29,30) ---
#define LOBOT_SERVO_OR_MOTOR_MODE_WRITE   29  // 写：设置舵机/电机模式
#define LOBOT_SERVO_OR_MOTOR_MODE_READ    30  // 读：读取当前模式

// --- 负载控制 (指令31,32) ---
#define LOBOT_SERVO_LOAD_OR_UNLOAD_WRITE  31  // 写：电机加载/卸载
#define LOBOT_SERVO_LOAD_OR_UNLOAD_READ   32  // 读：读取加载状态

// --- LED 控制 (指令33,34) ---
#define LOBOT_SERVO_LED_CTRL_WRITE        33  // 写：设置LED灯状态
#define LOBOT_SERVO_LED_CTRL_READ         34  // 读：读取LED灯状态

// --- 故障报警 (指令35,36) ---
#define LOBOT_SERVO_LED_ERROR_WRITE       35  // 写：设置故障报警类型
#define LOBOT_SERVO_LED_ERROR_READ        36  // 读：读取故障报警设置

// --- 距离 (指令48) ---
#define LOBOT_SERVO_DIS_READ              48  // 读：读取转动距离（脉冲数，32位无符号）


// ======================== 底层函数声明 ========================

/**
 * @brief  计算协议校验和
 * @param  buf 数据包缓冲区（包含帧头）
 * @return 校验和字节
 * @note   校验范围: buf[2] 至 buf[buf[3]+1]（即ID到数据末尾，不含校验和自身）
 */
byte LobotCheckSum(byte buf[]);

/**
 * @brief  接收并解析舵机返回的数据包
 * @param  SerialX 串口对象引用
 * @param  ret     输出缓冲区，存放有效数据（不含帧头/ID/长度/指令/校验和）
 * @return >0: 有效数据字节数, -1: 校验和错误, 0: 未收到完整包
 */
int LobotSerialServoReceiveHandle(HardwareSerial &SerialX, byte *ret);

#endif // SERIALSERVOINCLUDE_H
