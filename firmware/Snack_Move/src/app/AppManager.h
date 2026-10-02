#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <Arduino.h>
#include "../comm/Bluetooth.h"
#include "../comm/BinaryProtocol.h"

/*========================================================================
 * AppManager - 应用业务层
 *
 * 职责：
 *   1. 全局系统模式管理 (NORMAL / MANUAL_CALIB / ACTION_GROUP)
 *   2. 一站式初始化：BLE 二进制协议 + handler 注册 + 运动模块 + 舵机上电
 *
 * 系统模式互斥：任何时候只能处于一个模式。
 *========================================================================*/

// ======================== 系统模式 ========================

enum SystemMode {
    MODE_NORMAL = 0,          // 普通运动 / 字符串指令模式
    MODE_MANUAL_CALIB,        // 手动校准模式（实时调整、修改ID等）
    MODE_ACTION_GROUP         // 动作组模式（扫描姿态、执行动作组）
};

// ======================== 模式管理 ========================

/** @brief 获取当前系统模式 */
SystemMode getSystemMode();

/** @brief 设置系统模式（内部使用，外部通过 enterXxx/exitXxx 切换） */
void setSystemMode(SystemMode mode);

// ======================== 初始化 ========================

/**
 * @brief 一站式应用初始化
 *
 * 依次完成：
 *   1. BLE 二进制回调绑定
 *   2. 校准界面 handler 注册（6个）
 *   3. 动作组界面 handler 注册（5个）
 *   4. 运动模块初始化 (movementInit)
 *   5. 舵机上电
 *
 * @param ble     BLE 蓝牙对象引用
 * @param serial  舵机总线串口引用
 */
void appInit(Bluetooth &ble, HardwareSerial &serial);

#endif
