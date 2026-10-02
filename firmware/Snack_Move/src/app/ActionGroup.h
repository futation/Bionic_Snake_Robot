#ifndef ACTIONGROUP_H
#define ACTIONGROUP_H

#include <Arduino.h>
#include "../comm/BinaryProtocol.h"

/*========================================================================
 * ActionGroup - 动作组模式操作
 *
 * 处理二进制协议中动作组界面的全部 H→M 命令：
 *   0x0A  进入动作组模式
 *   0x0B  退出动作组模式
 *   0x0E  舵机掉电（允许手动掰动）
 *   0x0C  扫描舵机位置（上电 + 读取）
 *   0x02  执行静态动作组
 *========================================================================*/

// ======================== 模式切换 ========================

/** @brief 进入动作组模式（停止运动 → 设 MODE_ACTION_GROUP → 反馈 0x12） */
void enterActionGroupMode();

/** @brief 退出动作组模式（恢复 MODE_NORMAL → 反馈 0x13） */
void exitActionGroupMode();

// ======================== 动作组操作 ========================

/**
 * @brief 舵机掉电（所有舵机 setLoad=0）
 * 
 * 允许用户手动掰动舵机到目标姿态，配合 scanServoPositions 使用。
 * 仅在 MODE_ACTION_GROUP 下有效。
 */
void powerOffServos(HardwareSerial &serial);

/**
 * @brief 扫描舵机位置并上报
 *
 * 流程：上电 → 等待 200ms 稳定 → scanServoBus 扫描总线 →
 *       逐 ID 读取 LobotSerialServoReadPosition →
 *       bpSendScanResult(0x0D) 上报
 * 仅在 MODE_ACTION_GROUP 下有效。
 */
void scanServoPositions(HardwareSerial &serial);

/**
 * @brief 执行静态动作组
 *
 * 解析 Payload: [ID(1) Pos_L(1) Pos_H(1) Time_L(1) Time_H(1)] × count
 * 使用 moveAllSequentialIndividual 同步驱动所有舵机。
 * 仅在 MODE_ACTION_GROUP 下有效。
 *
 * @param data  原始 Payload 数据
 * @param count 舵机数量（每组 5 字节）
 */
void executeActionGroup(HardwareSerial &serial, const uint8_t* data, uint8_t count);

#endif
