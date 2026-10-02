#ifndef STRINGCOMMANDS_H
#define STRINGCOMMANDS_H

#include <Arduino.h>
#include "Bluetooth.h"

/*========================================================================
 * StringCommands - BLE 字符串命令路由模块
 *
 * 将 handleBluetoothCommand 中分散的 if/else 链封装为独立模块。
 * 与 BinaryProtocol 的回调注册模式一致：init 注册 + dispatch 入口。
 *
 * 暴露少量 getter 供主程序的 loop() 获取步态调度状态。
 *========================================================================*/

// ======================== 步态参数 ========================

struct GaitParams {
    float   ampVert     = 30.0f;
    float   ampHoriz    = 50.0f;
    float   speedVert   = 5.0f;
    float   speedHoriz  = 5.0f;
    float   phaseDiff   = 1.5708f;
    int16_t bias        = 30;
    bool    otherUnload = false;
    float   lambdaVert  = 0.0f;
    float   lambdaHoriz = 0.0f;
};

// 步态编号
enum {
    GAIT_NONE = 0,
    GAIT_VERT,
    GAIT_HORIZ,
    GAIT_DUAL,
};

// ======================== 初始化 ========================

/** @brief 初始化字符串命令模块（保存 BLE 和串口引用） */
void strCmdInit(Bluetooth &ble, HardwareSerial &serial);

// ======================== 命令入口 ========================

/** @brief 字符串命令分发入口（注册为 BLE 回调） */
void strCmdDispatch(String command);

// ======================== 步态调度 getter（供 loop() 使用） ========================

int  getPendingGait();
void clearPendingGait();
const GaitParams& getGaitParams();

// ======================== 舵机电源快捷操作 ========================

void strCmdLoadAllServos();
void strCmdUnloadAllServos();

#endif
