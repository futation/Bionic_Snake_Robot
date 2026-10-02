/**
 * SerialServoID.cpp - 舵机ID管理模块实现
 *
 * 指令13: SERVO_ID_WRITE - 写入新ID（掉电保存）
 * 指令14: SERVO_ID_READ  - 读取当前ID（广播模式）
 */

#include "SerialServoID.h"

/*========================================================================
 * LobotSerialServoSetID - 设置舵机ID
 *
 * 发送指令包：[0x55 0x55 oldID 4 13 newID Checksum]
 * 参数说明：
 *   - Length=4: Cmd(1) + newID(1) + Checksum(1) + 1 = 4
 *   - Cmd=13: SERVO_ID_WRITE
 *   - 新ID写入后立即生效并掉电保存
 *========================================================================*/
void LobotSerialServoSetID(HardwareSerial &SerialX, uint8_t oldID, uint8_t newID)
{
    byte buf[7];

    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;  // 帧头
    buf[2] = oldID;                                // 当前舵机ID
    buf[3] = 4;                                    // 数据长度
    buf[4] = LOBOT_SERVO_ID_WRITE;                 // 指令13
    buf[5] = newID;                                // 新ID
    buf[6] = LobotCheckSum(buf);                   // 校验和

    SerialX.write(buf, 7);
}

/*========================================================================
 * LobotSerialServoReadID - 读取舵机ID（广播模式）
 *
 * 发送指令包：[0x55 0x55 254 3 14 Checksum]
 * 使用广播ID=254，总线上只能有一个舵机，否则会发生冲突。
 *
 * 舵机返回：[0x55 0x55 ID 4 14 ID_byte Checksum]
 * 返回数据中 Data 部分为 ID_byte（1字节）
 *========================================================================*/
int LobotSerialServoReadID(HardwareSerial &SerialX)
{
    int count = 10000;
    byte buf[8];

    // --- 构建并发送读ID指令 ---
    buf[0] = buf[1] = LOBOT_SERVO_FRAME_HEADER;
    buf[2] = LOBOT_SERVO_ID_ALL;          // 广播ID=254
    buf[3] = 3;                            // 数据长度
    buf[4] = LOBOT_SERVO_ID_READ;          // 指令14
    buf[5] = LobotCheckSum(buf);
    SerialX.write(buf, 6);

    // --- 清空接收缓冲区 ---
    while (SerialX.available())
        SerialX.read();

    // --- 等待舵机响应 ---
    while (!SerialX.available()) {
        count -= 1;
        if (count < 0)
            return LOBOT_SERVO_ERR_TIMEOUT;
    }

    // --- 解析返回数据 ---
    int result = LobotSerialServoReceiveHandle(SerialX, buf);
    if (result > 0) {
        // 返回数据：buf[0] = ID_byte
        return (int)buf[0];
    } else if (result == LOBOT_SERVO_ERR_TIMEOUT) {
        return LOBOT_SERVO_ERR_TIMEOUT;
    } else {
        return LOBOT_SERVO_ERR_CHECKSUM;
    }
}
