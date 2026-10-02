/**
 * SerialServoInclude.cpp - 总线舵机协议底层实现
 *
 * 包含：校验和计算、数据包接收解析
 * 参考文档：《02+总线舵机通信协议.pdf》
 */

#include "SerialServoInclude.h"

// #define SERIAL_SERVO_DEBUG

/*========================================================================
 * LobotCheckSum - 计算协议校验和
 *
 * 校验范围：buf[2]（ID）到 buf[buf[3]+1]（最后一个数据字节）
 * 即跳过两个帧头 0x55，从 ID 开始累加 Length 个字节，
 * 然后取反得到校验和。
 *
 * 包结构：buf[0]=0x55 buf[1]=0x55 buf[2]=ID buf[3]=Length
 *         buf[4]=Cmd buf[5..]=Data buf[Length+2]=Checksum
 * 其中 Length = Cmd(1) + Data(N) + Checksum(1) + 1 = N + 3
 * 校验覆盖 buf[2]..buf[Length+1]，共 Length 字节
 *========================================================================*/
byte LobotCheckSum(byte buf[])
{
    byte i;
    uint16_t temp = 0;
    // 从 buf[2] (ID) 累加到 buf[buf[3]+1] (最后一个数据字节)
    // 共 buf[3] 个字节，不包含帧头和校验和自身
    for (i = 2; i < buf[3] + 2; i++) {
        temp += buf[i];
    }
    temp = ~temp;
    return (byte)temp;
}

/*========================================================================
 * LobotSerialServoReceiveHandle - 接收并解析舵机返回包
 *
 * 从串口读取一个完整的数据包，验证校验和，
 * 将纯数据部分（不含帧头/ID/长度/指令/校验和）复制到 ret。
 *
 * @param SerialX  串口对象引用
 * @param ret      输出缓冲区，存放纯数据字节
 * @return  >0: 有效数据字节数
 *          -1 (LOBOT_SERVO_ERR_TIMEOUT): 超时未收齐
 *          -2 (LOBOT_SERVO_ERR_CHECKSUM): 校验和错误
 *
 * 注意：调用前应确保串口有数据（由上层读函数保证）
 *========================================================================*/
int LobotSerialServoReceiveHandle(HardwareSerial &SerialX, byte *ret)
{
    byte recvBuf[32];   // 接收缓冲区（最大包长不超过32字节）
    byte idx = 0;       // 接收缓冲区索引
    int timeout;        // 超时计数器

    // ===== 第1步：检测帧头 =====
    // 需要连续收到两个LOBOT_SERVO_FRAME_HEADER
    byte headerCount = 0;
    
    // 等待第一个帧头
    timeout = 10000;  // 适当增加超时，因为帧头可能需要等待
    while (headerCount < 2) {
        // 检查是否有数据
        timeout = 10000;
        while (!SerialX.available()) {
            if (--timeout <= 0){
#ifdef SERIAL_SERVO_DEBUG
                Serial.println("等待帧头超时！");
#endif
                return LOBOT_SERVO_ERR_TIMEOUT;
            }
            delayMicroseconds(100);
        }
        
        byte incomingByte = SerialX.read();
        
        if (incomingByte == LOBOT_SERVO_FRAME_HEADER) {
            // 收到帧头
            recvBuf[idx++] = incomingByte;
            headerCount++;
            
            // 如果是第一个帧头，继续等待第二个
            if (headerCount == 1) {
                // 等待第二个帧头，但有限时间内
                timeout = 2000;  // 两个帧头之间的超时较短
                while (!SerialX.available()) {
                    if (--timeout <= 0) {
                        // 超时，重新开始
                        idx = 0;
                        headerCount = 0;
#ifdef SERIAL_SERVO_DEBUG
                        Serial.println("等待第二个帧头超时！");
#endif
                        return LOBOT_SERVO_ERR_TIMEOUT;
                    }
                    delayMicroseconds(100);
                }
                
                // 读取第二个字节
                incomingByte = SerialX.read();
                if (incomingByte == LOBOT_SERVO_FRAME_HEADER) {
                    recvBuf[idx++] = incomingByte;
                    headerCount++;
                } else {
                    // 第二个字节不是帧头，重新开始
                    idx = 0;
                    headerCount = 0;
                    // 注意：这里不返回错误，因为可能第一个帧头是数据的一部分
                    // 继续等待真正的帧头
                }
            }
        } else {
            // 收到的字节不是帧头，重置状态
            idx = 0;
            headerCount = 0;
        }
    }

#ifdef SERIAL_SERVO_DEBUG
    Serial.println("接收到数据包头！");
#endif
    
    // ===== 第2步：读取ID =====
    timeout = 5000;
    while (!SerialX.available()) {
        if (--timeout <= 0) 
        {
#ifdef SERIAL_SERVO_DEBUG
            Serial.println("等待ID超时！");
#endif
            return LOBOT_SERVO_ERR_TIMEOUT;
        }
        delayMicroseconds(100);
    }
    recvBuf[idx++] = SerialX.read();  // recvBuf[2] = ID
     // ===== 第3步：读取长度 =====
    timeout = 5000;
    while (!SerialX.available()) {
        if (--timeout <= 0) {
#ifdef SERIAL_SERVO_DEBUG
            Serial.println("等待长度超时！");
#endif
            return LOBOT_SERVO_ERR_TIMEOUT;
        }
        delayMicroseconds(100);
    }
    recvBuf[idx++] = SerialX.read();  // recvBuf[3] = Length

    byte dataLen = recvBuf[3];  // Length 字段值

    // 长度合法性检查：最小为3（仅有Cmd+Checksum情况），最大不超过30
    if (dataLen < 3 || dataLen > 10) {
#ifdef SERIAL_SERVO_DEBUG
        Serial.print("非法数据长度:");
        Serial.println(dataLen);
#endif
        return LOBOT_SERVO_ERR_CHECKSUM;
    }

    // ===== 第4步：读取剩余字节 =====
    //   需要再读取 (dataLen - 1) 个字节：
    //   Length 之后的内容为 Cmd(1) + Data(N) + Checksum(1) = dataLen + 1 - 2 字节
    //   最终 recvBuf 填充到索引 dataLen+2 (Checksum 位置)
    for (byte i = 0; i < dataLen - 1; i++) {
        timeout = 5000;
        while (!SerialX.available()) {
            if (--timeout <= 0) {
#ifdef SERIAL_SERVO_DEBUG
                Serial.println("等待剩余数据超时！");
#endif
                return LOBOT_SERVO_ERR_TIMEOUT;
            }
            delayMicroseconds(100);
        }
        recvBuf[idx++] = SerialX.read();
    }

    // --- 第4步：校验和验证 ---
    // Checksum 位于 recvBuf[dataLen + 2]
    // 索引从0开始，所以是dataLen+3-1
    if (LobotCheckSum(recvBuf) != recvBuf[dataLen + 2]) {
#ifdef SERIAL_SERVO_DEBUG
        Serial.println("校验和错误！");
#endif
        return LOBOT_SERVO_ERR_CHECKSUM;
    }

#ifdef SERIAL_SERVO_DEBUG
    Serial.print("[DEBUG] √ 完整接收数据包 (");
    Serial.print(idx, DEC);
    Serial.print(" 字节): ");
    for (byte i = 0; i < idx; i++) {
        Serial.print(recvBuf[i], HEX);
        if (i < idx - 1) Serial.print(" ");
    }
    Serial.println();
#endif

    // --- 第5步：提取纯数据 ---
    // 有效数据从 recvBuf[5] 开始（跳过：帧头×2、ID、Length、Cmd）
    // 数据字节数 = dataLen(数据长度) - 3(Cmd(1) + ID(1) + Length(1)) 
    if (dataLen > 0) {
        memcpy(ret, recvBuf + 5, dataLen - 3);
    }

#ifdef SERIAL_SERVO_DEBUG
    Serial.print("[DEBUG] 返回的数据包 (");
    Serial.print(dataLen - 3, DEC);
    Serial.print(" 字节): ");
    for (byte i = 0; i < dataLen - 3; i++) {
        Serial.print(ret[i], HEX);
        if (i < dataLen - 1 - 3) Serial.print(" ");
    }
    Serial.println();
#endif
    return dataLen - 3;
}
