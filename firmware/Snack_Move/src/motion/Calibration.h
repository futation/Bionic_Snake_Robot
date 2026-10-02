#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "../servo/ServoRegistry.h"

/*========================================================================
 * Calibration - 舵机校准扫描模块
 *
 * 提供校准扫描、ID 重排、角度偏差校准等功能。
 *========================================================================*/

// ======================== 校准参数 ========================

#define CALIB_CENTER_POS      500       // 期望中心位置脉冲
#define CALIB_OFFSET_LIMIT    125       // 角度偏差极限 (±125)
#define CALIB_MOVE_TIME       800       // 校准移动时间 (ms)

// ======================== API ========================

/**
 * @brief 执行完整的校准扫描流程
 *
 * 前提：用户已将蛇形机器人所有关节摆正到回正状态。
 *
 * 流程：
 *   1. 确保舵机上电
 *   2. 扫描 ID 0~20，收集 (旧ID, 物理中心位置)
 *   3. 检查 ID 是否连续 (0..N-1)，不连续则按序重命名
 *   4. 分两轮校准角度偏差（奇数ID先，偶数ID后）
 *   5. 每轮：设 Position=500，用角度偏差补偿物理中心与 500 的差距
 *   6. 超限检测并警告
 *   7. 验证读取
 *   8. 保存到 EEPROM 校准槽 (SLOT_CALIB)
 *
 * @param serial  舵机总线串口
 * @param ble     蓝牙对象指针（用于回传进度），可为 nullptr
 */
void calibrateAllServos(HardwareSerial &serial, class Bluetooth *ble);

/**
 * @brief 扫描总线上所有舵机，返回 (ID, Position)
 *
 * 遍历 ID 0~20，读取每个舵机的位置。
 * 结果天然有序（遍历顺序即 ID 升序）。
 *
 * @param serial       舵机总线串口
 * @param idsOut       输出：舵机ID数组（调用者分配，至少 MAX_REGISTERED_SERVOS 字节）
 * @param positionsOut 输出：舵机位置数组（调用者分配）
 * @return 找到的舵机数量
 */
uint8_t scanServoBus(HardwareSerial &serial, uint8_t *idsOut, int16_t *positionsOut);

/**
 * @brief 将舵机 ID 按序重排为 0,1,2,...,N-1
 *
 * 安全保证：扫描结果已排序，旧ID ≥ 新ID，不会冲突。
 * 如果 ID 已连续（id[i] == i 对所有 i），跳过重命名。
 *
 * @param serial   舵机总线串口
 * @param oldIDs   当前ID列表（已从小到大排序）
 * @param count    舵机数量
 * @return 实际重命名的舵机数量（如果已连续则返回 0）
 */
uint8_t renumberServoIDs(HardwareSerial &serial, const uint8_t *oldIDs, uint8_t count);

/**
 * @brief 校准单个舵机的角度偏差
 *
 * 将舵机移到 Position=500，计算 offset = physicalCenter - 500，
 * 钳位到 ±125 后写入舵机并保存。
 * 若偏移超限（如 phys=300, 需要-200 但仅能-125），
 * 剩余误差通过调整脉冲位置补偿（500→425），确保最终到位。
 *
 * @param serial         舵机总线串口
 * @param id             舵机ID
 * @param physicalCenter 物理中心位置（扫描读到的值）
 * @param outPosition    输出：补偿后的目标脉冲位置（用于写入 EEPROM）
 * @return true=偏差在 ±125 内, false=超限（已通过位置补偿）
 */
bool calibrateSingleOffset(HardwareSerial &serial,  Bluetooth* ble , uint8_t id,
                           int16_t physicalCenter, int16_t *outPosition);

#endif
