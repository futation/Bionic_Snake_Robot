/**
 * SerialServoMove.cpp - 舵机运动控制模块实现
 *
 * 包含所有运动相关指令的发送与读取函数。
 * 参考文档：《02+总线舵机通信协议.pdf》
 *
 * 指令清单：
 *   1  - SERVO_MOVE_TIME_WRITE      立即移动
 *   2  - SERVO_MOVE_TIME_READ       读取移动参数
 *   7  - SERVO_MOVE_TIME_WAIT_WRITE 预设移动
 *   8  - SERVO_MOVE_TIME_WAIT_READ  读取预设参数
 *   11 - SERVO_MOVE_START           启动预设
 *   12 - SERVO_MOVE_STOP            停止运动
 *   28 - SERVO_POS_READ             读取位置
 *   29 - SERVO_OR_MOTOR_MODE_WRITE  设置工作模式
 *   31 - SERVO_LOAD_OR_UNLOAD_WRITE 设置加载/卸载
 *   48 - SERVO_DIS_READ             读取距离
 */

#include "SerialServoMove.h"

/*========================================================================
 * LobotSerialServoMove - 立即控制舵机转动（指令1）
 *
 * 发送指令包：[0x55 0x55 ID 7 1 Pos_L Pos_H Time_L Time_H Checksum]
 * 参数范围：position: 0~1000, time: 0~30000 (ms)
 *========================================================================*/
void LobotSerialServoMove(HardwareSerial &SerialX, uint8_t id, int16_t position, uint16_t time)
{
    byte buf[10];

    // 参数范围约束
    if (position < 0)   position = 0;
    if (position > 1000) position = 1000;
    if (time > 30000)  time = 30000;
     
    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 7;                                // Length = N+3 = 4+3 = 7
    buf[4] = LOBOT_SERVO_MOVE_TIME_WRITE;      // Cmd=1
    buf[5] = GET_LOW_BYTE(position);
    buf[6] = GET_HIGH_BYTE(position);
    buf[7] = GET_LOW_BYTE(time);
    buf[8] = GET_HIGH_BYTE(time);
    buf[9] = LobotCheckSum(buf);

    SerialX.write(buf, 10);
}

/*========================================================================
 * LobotSerialServoMoveTimeWait - 预设位置（指令7）
 *
 * 发送指令包：[0x55 0x55 ID 7 7 Pos_L Pos_H Time_L Time_H Checksum]
 * 与立即移动的区别：舵机收到后不会立即执行，需等待 START 指令(11)
 *
 * 由于舵机硬件不支持指令8回读预设参数，本函数在驱动层需要多加一层封装
 * 在发送指令后会将预设值同步写入 SingleServoManager 内部成员，
 * 同时需要添加一点延时
 * 实现软件层维护预设参数。
 *========================================================================*/
void LobotSerialServoMoveTimeWait(HardwareSerial &SerialX, uint8_t id, int16_t position, uint16_t time)
{
    byte buf[10];

    if (position < 0)   position = 0;
    if (position > 1000) position = 1000;
    if (time > 30000)  time = 30000;

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 7;
    buf[4] = LOBOT_SERVO_MOVE_TIME_WAIT_WRITE;  // Cmd=7
    buf[5] = GET_LOW_BYTE(position);
    buf[6] = GET_HIGH_BYTE(position);
    buf[7] = GET_LOW_BYTE(time);
    buf[8] = GET_HIGH_BYTE(time);
    buf[9] = LobotCheckSum(buf);

    SerialX.write(buf, 10);
}

/*========================================================================
 * LobotSerialServoMoveStart - 启动预设移动（指令11）
 * 注意，该指令和预设移动预设指令之间要有一定的时间间隔，否则舵机可能来不及处理预设指令就收到了启动指令，导致启动失败
 * 启动预设指令后，舵机内部预设寄存器的值不会变，所以可以预设一次，无限次启动
 * 发送指令包：[0x55 0x55 ID 3 11 Checksum]
 *========================================================================*/
void LobotSerialServoMoveStart(HardwareSerial &SerialX, uint8_t id)
{
    byte buf[6];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = LOBOT_SERVO_MOVE_START;  // Cmd=11
    buf[5] = LobotCheckSum(buf);

    SerialX.write(buf, 6);
}

/*========================================================================
 * LobotSerialServoStopMove - 停止运动（指令12）
 * 注意：该指令只能停止舵机模式，电机模式下该指令无关停止电机
 * 发送指令包：[0x55 0x55 ID 3 12 Checksum]
 *========================================================================*/
void LobotSerialServoStopMove(HardwareSerial &SerialX, uint8_t id)
{
    byte buf[6];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = LOBOT_SERVO_MOVE_STOP;  // Cmd=12
    buf[5] = LobotCheckSum(buf);

    SerialX.write(buf, 6);
}

/*========================================================================
 * LobotSerialServoReadPosition - 读取当前位置（指令28）
 *
 * 请求：[0x55 0x55 ID 3 28 Checksum]
 * 响应：[0x55 0x55 ID 5 28 Pos_L Pos_H Checksum]
 * 返回数据：2字节（位置值，小端序）
 *========================================================================*/
int LobotSerialServoReadPosition(HardwareSerial &SerialX, uint8_t id)
{
    int count = 10000;
    byte buf[6];

    // --- 构建请求 ---
    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = LOBOT_SERVO_POS_READ;  // Cmd=28
    buf[5] = LobotCheckSum(buf);
    SerialX.write(buf, 6);

    // --- 清空缓冲区 ---
    while (SerialX.available())
        SerialX.read();

    // --- 等待响应 ---
    while (!SerialX.available()) {
        if (--count <= 0)
            return LOBOT_SERVO_ERR_TIMEOUT;
    }

    // --- 解析 ---
    int result = LobotSerialServoReceiveHandle(SerialX, buf);
    if (result > 0) {
        // buf[0]=Pos_L, buf[1]=Pos_H
        return (int16_t)BYTE_TO_HW(buf[1], buf[0]);
    } else if (result == LOBOT_SERVO_ERR_TIMEOUT) {
        return LOBOT_SERVO_ERR_TIMEOUT;
    } else {
        return LOBOT_SERVO_ERR_CHECKSUM;
    }
}

/*========================================================================
 * LobotSerialServoReadMoveTime - 读取立即移动参数（指令2）
 *
 * 请求：[0x55 0x55 ID 3 2 Checksum]
 * 响应：[0x55 0x55 ID 7 2 Pos_L Pos_H Time_L Time_H Checksum]
 * 返回数据：4字节（Position 2B + Time 2B）
 * 注意：这里返回的是读取成功与否，而不是读取结果数据
 *========================================================================*/
int LobotSerialServoReadMoveTime(HardwareSerial &SerialX, uint8_t id, int16_t *position, uint16_t *time)
{
    int count = 10000;
    byte buf[8];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 3;
    buf[4] = LOBOT_SERVO_MOVE_TIME_READ;  // Cmd=2
    buf[5] = LobotCheckSum(buf);
    SerialX.write(buf, 6);

    while (SerialX.available())
        SerialX.read();

    while (!SerialX.available()) {
        if (--count <= 0) return LOBOT_SERVO_ERR_TIMEOUT;
    }

    int result = LobotSerialServoReceiveHandle(SerialX, buf);
    if (result == 4) {
        *position = (int16_t)BYTE_TO_HW(buf[1], buf[0]);
        *time     = (uint16_t)BYTE_TO_HW(buf[3], buf[2]);
        return LOBOT_SERVO_OK;
    } else if (result == LOBOT_SERVO_ERR_TIMEOUT) {
        return LOBOT_SERVO_ERR_TIMEOUT;
    } else {
        return LOBOT_SERVO_ERR_CHECKSUM;
    }
}

/*
读取预设舵机移动参数，经过测试发现舵机对该指令没有返回，猜测该指令只在特殊的舵机才生效
但蛇型机器人上的舵机都是不支持的
所以,这里我们不做编译
实际上,我们也不需要读取预设参数,因为我们在预设的时候就有这些参数了,没有必要再读一次
*/

// /*========================================================================
//  * LobotSerialServoReadMoveTimeWait - 读取预设移动参数（指令8）
//  *
//  * 请求：[0x55 0x55 ID 3 8 Checksum]
//  * 响应：[0x55 0x55 ID 7 8 Pos_L Pos_H Time_L Time_H Checksum]
//  * 返回数据：4字节（Position 2B + Time 2B）
//  *========================================================================*/
// int LobotSerialServoReadMoveTimeWait(HardwareSerial &SerialX, uint8_t id, int16_t *position, uint16_t *time)
// {
//     int count = 10000;
//     byte buf[6];

//     buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
//     buf[2] = id;
//     buf[3] = 3;
//     buf[4] = LOBOT_SERVO_MOVE_TIME_WAIT_READ;  // Cmd=8
//     buf[5] = LobotCheckSum(buf);
//     SerialX.write(buf, 6);

//     while (SerialX.available())
//         SerialX.read();

//     while (!SerialX.available()) {
//         if (--count <= 0) return LOBOT_SERVO_ERR_TIMEOUT;
//     }

//     int result = LobotSerialServoReceiveHandle(SerialX, buf);

//     Serial.print("======读取结果======：");
//     Serial.println(result);
    
//     if (result > 0) {
//         *position = (int16_t)BYTE_TO_HW(buf[1], buf[0]);
//         *time     = (uint16_t)BYTE_TO_HW(buf[3], buf[2]);
//         return LOBOT_SERVO_OK;
//     } else if (result == LOBOT_SERVO_ERR_TIMEOUT) {
//         return LOBOT_SERVO_ERR_TIMEOUT;
//     } else {
//         return LOBOT_SERVO_ERR_CHECKSUM;
//     }
// }

/*========================================================================
 * LobotSerialServoSetMode - 设置工作模式（指令29）
 *
 * 发送：[0x55 0x55 ID 7 29 Mode 0 Speed_L Speed_H Checksum]
 * mode=0: 位置伺服模式（舵机模式），此时 speed 参数无意义（可传0）
 * mode=1: 电机调速模式，speed 控制转速（-1000~1000）
 * 注意：协议中第2个数据字节为保留字节（固定传0）
 *========================================================================*/
void LobotSerialServoSetMode(HardwareSerial &SerialX, uint8_t id, uint8_t mode, int16_t speed)
{
    byte buf[10];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 7;                                  // Length = N+3 = 3+3+1? = 7
    buf[4] = LOBOT_SERVO_OR_MOTOR_MODE_WRITE;    // Cmd=29
    buf[5] = mode;                               // 模式：0=位置, 1=电机
    buf[6] = 0;                                  // 保留字节
    buf[7] = GET_LOW_BYTE((uint16_t)speed);
    buf[8] = GET_HIGH_BYTE((uint16_t)speed);
    buf[9] = LobotCheckSum(buf);

    SerialX.write(buf, 10);
}

/*========================================================================
 * LobotSerialServoSetLoad - 设置电机加载/卸载（指令31）
 *
 * 发送：[0x55 0x55 ID 4 31 Load Checksum]
 * load=0: 卸载电机，无力矩输出
 * load=1: 加载电机，有力矩输出
 *========================================================================*/
void LobotSerialServoSetLoad(HardwareSerial &SerialX, uint8_t id, uint8_t load)
{
    byte buf[7];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = id;
    buf[3] = 4;                                  // Length = N+3 = 1+3 = 4
    buf[4] = LOBOT_SERVO_LOAD_OR_UNLOAD_WRITE;   // Cmd=31
    buf[5] = load;                               // 0=卸载, 1=加载
    buf[6] = LobotCheckSum(buf);

    SerialX.write(buf, 7);
}

/* 阅读总线舵机协议手册，对该指令的描述显得模棱两可
    经过测试发现舵机对该指令没有返回，猜测该指令只在特殊的舵机才生效
    但蛇型机器人上的舵机都是不支持的
    所以,这里我们不做编译
*/

// /*========================================================================
//  * LobotSerialServoReadDistance - 读取转动距离（指令48）
//  *
//  * 请求：[0x55 0x55 ID 3 48 Checksum]
//  * 响应：[0x55 0x55 ID 7 48 Dist_L Dist_H Dist_HH Dist_HHH Checksum]  (小端序)
//  * 返回数据：4字节（32位无符号整数，脉冲数）
//  *
//  * 注意：返回类型为 int，无法表示完整的 uint32_t 范围。
//  *       对精度有要求时请使用 LobotSerialServoReadDistanceU32()
//  *========================================================================*/
// int LobotSerialServoReadDistance(HardwareSerial &SerialX, uint8_t id)
// {
//     int count = 10000;
//     byte buf[8];

//     buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
//     buf[2] = id;
//     buf[3] = 3;
//     buf[4] = LOBOT_SERVO_DIS_READ;  // Cmd=48
//     buf[5] = LobotCheckSum(buf);
//     SerialX.write(buf, 6);

//     while (SerialX.available())
//         SerialX.read();

//     while (!SerialX.available()) {
//         if (--count <= 0) return LOBOT_SERVO_ERR_TIMEOUT;
//     }

//     int result = LobotSerialServoReceiveHandle(SerialX, buf);
//     if (result == 4) {
//         // 4字节小端序 → uint32_t
//         uint32_t dist = (uint32_t)buf[0]
//                       | ((uint32_t)buf[1] << 8)
//                       | ((uint32_t)buf[2] << 16)
//                       | ((uint32_t)buf[3] << 24);
//         return (int)dist;  // 注意：可能溢出为负数
//     } else if (result == LOBOT_SERVO_ERR_TIMEOUT) {
//         return LOBOT_SERVO_ERR_TIMEOUT;
//     } else {
//         return LOBOT_SERVO_ERR_CHECKSUM;
//     }
// }




