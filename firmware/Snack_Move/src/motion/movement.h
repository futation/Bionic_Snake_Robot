#ifndef MOVEMENT_H
#define MOVEMENT_H

#include <Arduino.h>
#include "../servo/SerialServoControl.h"
#include "../servo/ServoRegistry.h"

// ======================== 常数 ========================
#define PULSE_PER_DEGREE       (1000.0f / 240.0f)   // 脉冲/度
#define PI                     3.14159265358979323846

// ======================== 全局可配置变量 ========================

/** @brief 摆位默认运动时间 (ms)，可直接读写，初始值 500 */
extern uint16_t g_defaultMoveTime;

/** @brief 正弦波波长（关节间隔数），一个完整波形跨越的关节间隔数。
 *  movementInit 自动计算为 2*(总舵机数-1)，可覆盖 */
extern float g_lambda;

// ======================== 初始化 ========================

/**
 * @brief 初始化运动模块（必须在 setup() 中调用）
 *
 * 流程：
 *   1. 初始化舵机池 ServoBus
 *   2. 从 EEPROM 加载校准槽 (SLOT_CALIB)
 *   3. 若校准槽为空 → 自动触发 calibrateAllServos()
 *   4. 将校准数据拷贝到模块内部（舵机数、ID列表、中心脉冲）
 *   5. 设置 ServoBus 总线信息
 *
 * @param serial  舵机总线串口
 * @param ble     蓝牙对象指针（校准回传用），可为 nullptr
 * @return true=初始化成功, false=失败（无舵机）
 */
bool movementInit(HardwareSerial &serial, class Bluetooth *ble = nullptr);

// ======================== 状态查询 ========================

/** @brief 获取总线上舵机数量 */
uint8_t getServoCount();

/** @brief 获取指定索引的舵机 ID（0 ~ count-1） */
uint8_t getServoId(uint8_t index);

/** @brief 获取指定舵机的中心脉冲 */
int16_t getCenterPulse(uint8_t servoId);

/** @brief 检查运动模块是否已初始化 */
bool isMovementReady();

// ======================== 中心基准管理 ========================

/**
 * @brief 从指定槽位重新加载中心位置数据到运动模块内部数组
 *
 * 调用前须确保对应槽位已 loadRegistry 且数据有效。
 * 更新 g_servoCount / g_servoIds / g_centerPulses，
 * 联动更新 ServoBus 和极限参数。
 *
 * @param slot  槽位编号 (SLOT_CALIB=0, SLOT_SCAN1=1, SLOT_SCAN2=2)
 * @param ble   蓝牙指针（用于日志），可为 nullptr
 * @return true=成功, false=注册表为空或无效
 */
bool reloadCenterFromSlot(uint8_t slot, class Bluetooth *ble = nullptr);

/** @brief 获取当前中心位置数据来源槽位 */
uint8_t getCenterSourceSlot();

// ======================== 摆位函数 ========================

/**
 * @brief 所有舵机复位到中心位置（基于 SLOT_CALIB 数据）
 *
 * 采用逐个立即驱动方式，覆盖当前正在执行的动作。
 *
 * @param serial  舵机总线串口
 */
void resetToCenter(HardwareSerial &serial);

/**
 * @brief 所有舵机摆到指定槽位保存的位姿
 *
 * 重新从 EEPROM 加载对应槽位，将舵机逐一驱动到记录位置。
 * 仅接受 SLOT_SCAN1 或 SLOT_SCAN2。
 *
 * @param serial  舵机总线串口
 * @param slot    槽位编号 (SLOT_SCAN1 或 SLOT_SCAN2)
 */
void moveToPose(HardwareSerial &serial, uint8_t slot);

// ======================== 步态控制 ========================

/** @brief 设置正弦步态单步运动时间 (ms)，影响最大波速计算 */
void setSineMoveTime(uint16_t time_ms);

/** @brief 查询当前正弦步态单步运动时间 (ms) */
uint16_t getSineMoveTime();

/** @brief 覆盖波长 lambda，联动更新 g_maxWaveSpeed
 *  @param lambda 新波长（关节数），必须 > 0 */
void setLambda(float lambda);

/** @brief 查询当前波长 lambda（关节数） */
float getLambda();

/** @brief 查询系统最大允许振幅（度），基于所有舵机中心脉冲计算 */
float getMaxAmplitudeDeg();

/** @brief 查询最大波传播速度（关节间隔/秒），基于当前 lambda 与 sineMoveTime 计算 */
float getMaxWaveSpeed();

/** @brief 查询 lambda 推荐范围下限（关节间隔数），基于运动组关节数 (总舵机数/2) 计算 */
float getLambdaMin();

/** @brief 查询 lambda 推荐范围上限（关节间隔数），基于运动组关节数 (总舵机数/2) 计算 */
float getLambdaMax();

/** @brief 停止当前正在运行的步态运动 */
void stopGait();

/**
 * @brief 垂直正弦步态 — 驱动偶数 ID 舵机组（0,2,4,6,...）
 *
 * 采用直接单舵机立即驱动。方向由 wave_speed 符号决定：正=前进，负=后退。
 * 振幅和速度不做钳位，由用户自行负责安全范围。
 *
 * @param serial        舵机总线串口
 * @param amplitude_deg 振幅（度）
 * @param wave_speed    波浪传播速度（关节间隔/秒），正负号决定方向
 * @param lambda        波长（关节间隔数），0=使用全局 g_lambda 默认值
 * @param bias_deg      偏置角（度），正/负=向一侧偏转，0=直行
 * @param otherUnload   true=运动期间将另一组舵机掉电
 */
void sineGaitVertical(HardwareSerial &serial, float amplitude_deg, float wave_speed,
                      float lambda = 0.0f, int16_t bias_deg = 0, bool otherUnload = false);

/**
 * @brief 水平正弦步态 — 驱动奇数 ID 舵机组（1,3,5,7,...）
 *
 * 采用直接单舵机立即驱动。方向由 wave_speed 符号决定：正=前进，负=后退。
 * 振幅和速度不做钳位，由用户自行负责安全范围。
 *
 * @param serial        舵机总线串口
 * @param amplitude_deg 振幅（度）
 * @param wave_speed    波浪传播速度（关节间隔/秒），正负号决定方向
 * @param lambda        波长（关节间隔数），0=使用全局 g_lambda 默认值
 * @param bias_deg      偏置角（度），正/负=向一侧偏转，0=直行
 * @param otherUnload   true=运动期间将另一组舵机掉电
 */
void sineGaitHorizontal(HardwareSerial &serial, float amplitude_deg, float wave_speed,
                        float lambda = 0.0f,int16_t bias_deg = 0, bool otherUnload = false);

/**
 * @brief 双波合成步态 — 奇偶舵机组同时做正弦运动
 *
 * 两组合成正交正弦波（如垂直+水平），相位差可调。
 * 采用直接单舵机立即驱动全部舵机。
 * 振幅和速度不做钳位，由用户自行负责安全范围。
 *
 * @param serial           舵机总线串口
 * @param amplitude_vert   垂直（偶数）振幅（度）
 * @param amplitude_horiz  水平（奇数）振幅（度）
 * @param wave_speed_vert  垂直波速（关节间隔/秒）
 * @param wave_speed_horiz 水平波速（关节间隔/秒）
 * @param lambda_vert      垂直波长（关节间隔数），0=使用全局 g_lambda
 * @param lambda_horiz     水平波长（关节间隔数），0=使用全局 g_lambda
 * @param phase_diff       两波相位差（弧度），默认 π/2
 */
void sineGaitDual(HardwareSerial &serial,
                  float amplitude_vert, float amplitude_horiz,
                  float wave_speed_vert, float wave_speed_horiz,
                  float lambda_vert = 0.0f,float lambda_horiz = 0.0f,                   
                  float phase_diff = PI / 2.0f);

#endif