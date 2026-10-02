#ifndef BINARYPROTOCOL_H
#define BINARYPROTOCOL_H

#include <Arduino.h>
#include <stdint.h>

/*========================================================================
 * BinaryProtocol - 上位机 ↔ 下位机 二进制通信协议 v2.0
 *
 * 帧格式：
 *   | 0xAA | Cmd | Len | Payload[0..Len-1] | CS | 0x55 |
 *
 * CS = 0xFF - ((Cmd + Len + ΣPayload) & 0xFF)
 *
 * 与舵机总线协议 (0x55 0x55 ...) 完全独立，通过 BLE 传输。
 *========================================================================*/

// ======================== 协议常量 ========================

#define BP_HEADER       0xAA    // 帧头
#define BP_FOOTER       0x55    // 帧尾
#define BP_MAX_PAYLOAD  128     // 最大 Payload 长度

// ======================== 命令码 ========================

// --- 校准界面相关 ---
#define BP_CMD_SET_SERVO           0x01  // H→M: 实时调整单舵机       Payload: ID(1)+Pos(2)+Offset(1) Len=4
#define BP_CMD_SAVE_CALIB          0x03  // H→M: 保存校准到EEPROM     Payload: ID(1)+Pos(2)+Offset(1) Len=4
#define BP_CMD_FEEDBACK            0x04  // M→H: 通用操作反馈
#define BP_CMD_ENTER_MANUAL_CALIB  0x05  // H→M: 进入手动校准模式     Len=0
#define BP_CMD_REPORT_ALL_SERVOS   0x06  // M→H: 上报所有舵机参数     Payload: [ID+Pos+Offset]×N
#define BP_CMD_CHANGE_ID           0x07  // H→M: 修改舵机ID           Payload: oldID(1)+newID(1) Len=2
#define BP_CMD_READ_ALL_SERVOS     0x08  // H→M: 读取所有舵机参数     Len=0
#define BP_CMD_EXIT_MANUAL_CALIB   0x09  // H→M: 退出手动校准模式     Len=0

// --- 动作组界面相关 ---
#define BP_CMD_ACTION_GROUP        0x02  // H→M: 执行静态动作组       Payload: [ID+Pos+Time]×N
#define BP_CMD_ENTER_ACTION_GROUP  0x0A  // H→M: 进入动作组模式       Len=0
#define BP_CMD_EXIT_ACTION_GROUP   0x0B  // H→M: 退出动作组模式       Len=0
#define BP_CMD_SCAN_SERVOS         0x0C  // H→M: 扫描舵机位置         Len=0
#define BP_CMD_REPORT_SCAN_RESULT  0x0D  // M→H: 上报扫描结果         Payload: [ID+Pos]×N
#define BP_CMD_POWER_OFF_SERVOS    0x0E  // H→M: 舵机掉电             Len=0

// --- 运动参数查询 ---
#define BP_CMD_QUERY_MOTION_LIMITS  0x0F  // H→M: 查询运动极限参数     Len=0
#define BP_CMD_REPORT_MOTION_LIMITS 0x10  // M→H: 上报运动极限参数     Payload: Amax(4)+Vmax(4)+λmin(4)+λmax(4)+λcur(4)+Time(2) Len=22

// ======================== 反馈状态码（0x04 的 Payload[0]）========================

#define BP_FB_GENERAL_OK           0x00  // 通用操作成功（无特定语义）
#define BP_FB_READ_PARAMS_OK       0x01  // 参数读取成功              （响应 0x08）
#define BP_FB_SAVE_PARAMS_OK       0x02  // 参数保存成功              （响应 0x03）
#define BP_FB_CHANGE_ID_OK         0x03  // ID修改成功                （响应 0x07）
#define BP_FB_ENTER_MANUAL_OK      0x10  // 进入手动校准模式成功      （响应 0x05）
#define BP_FB_EXIT_MANUAL_OK       0x11  // 退出手动校准模式成功      （响应 0x09）
#define BP_FB_ENTER_ACTION_OK      0x12  // 进入动作组模式成功        （响应 0x0A）
#define BP_FB_EXIT_ACTION_OK       0x13  // 退出动作组模式成功        （响应 0x0B）
#define BP_FB_POWER_OFF_OK         0x14  // 舵机掉电成功              （响应 0x0E）
#define BP_FB_GENERAL_FAIL         0xFF  // 通用失败                  （Payload[1]=原因码）

// ======================== 失败原因码（Payload[0]==0xFF 时的 Payload[1]）========================

#define BP_ERR_CHECKSUM_FORMAT     0x01  // 校验和/命令长度错误
#define BP_ERR_INVALID_PARAM       0x02  // 未知命令参数值/舵机数超量
#define BP_ERR_TIMEOUT_NO_RESP     0x03  // 执行超时/舵机无响应
#define BP_ERR_MODE_MISMATCH       0x04  // 模式不匹配/未注册处理函数
#define BP_ERR_HARDWARE_STATE      0x05  // 硬件状态不允许
#define BP_ERR_NO_SERVOS           0x06  // 舵机数量为零

// ======================== 回调函数类型 ========================

typedef void (*BPCallback_Void)();
typedef void (*BPCallback_SetServo)(uint8_t id, int16_t position, int8_t offset);
typedef void (*BPCallback_ActionGroup)(const uint8_t* data, uint8_t count);
typedef void (*BPCallback_ChangeID)(uint8_t oldID, uint8_t newID);
typedef void (*BPCallback_SendData)(const uint8_t* data, uint16_t len);

// ======================== API ========================

/** @brief 注册数据发送回调（通过 BLE 发送二进制数据） */
void bpSetSendCallback(BPCallback_SendData sendFn);

// --- 注册 H→M 命令的处理回调 ---
void bpSetSetServoHandler(BPCallback_SetServo cb);
void bpSetSaveCalibHandler(BPCallback_SetServo cb);
void bpSetActionGroupHandler(BPCallback_ActionGroup cb);
void bpSetEnterManualCalibHandler(BPCallback_Void cb);
void bpSetExitManualCalibHandler(BPCallback_Void cb);
void bpSetChangeIDHandler(BPCallback_ChangeID cb);
void bpSetReadAllServosHandler(BPCallback_Void cb);
void bpSetEnterActionGroupHandler(BPCallback_Void cb);
void bpSetExitActionGroupHandler(BPCallback_Void cb);
void bpSetScanServosHandler(BPCallback_Void cb);
void bpSetPowerOffServosHandler(BPCallback_Void cb);
void bpSetQueryMotionLimitsHandler(BPCallback_Void cb);

/** @brief 解析收到的完整二进制帧（由 BLE 层调用，不含分包处理） */
void bpParsePacket(const uint8_t* data, uint16_t len);

// --- 构建 & 发送（M→H） ---

/** @brief 计算校验和 */
uint8_t bpCalcChecksum(uint8_t cmd, uint8_t len, const uint8_t* payload);

/** @brief 构建完整帧到 outBuf，返回帧总长度 */
uint16_t bpBuildPacket(uint8_t cmd, const uint8_t* payload, uint8_t len, uint8_t* outBuf);

/** @brief 发送成功反馈 (0x04, Len=1) */
void bpSendFeedback(uint8_t statusCode);

/** @brief 发送失败反馈（带原因码, 0x04, Len=2, Payload=[0xFF, reason]） */
void bpSendFeedbackFail(uint8_t reasonCode);

/** @brief 发送「上报所有舵机参数」响应包 (0x06)，Payload 不含 Count 前缀 */
void bpSendReportAllServos(uint8_t count, const uint8_t* ids,
                           const int16_t* positions, const int8_t* offsets);

/** @brief 发送「上报扫描结果」响应包 (0x0D)，Payload: [ID+Pos]×N */
void bpSendScanResult(uint8_t count, const uint8_t* ids, const int16_t* positions);

/** @brief 发送「上报运动极限参数」响应包 (0x10)
 *  Payload(22B): Amax(4f)+Vmax(4f)+λmin(4f)+λmax(4f)+λcur(4f)+sineTime(2B) */
void bpSendMotionLimits(float aMax, float vMax, float lambdaMin, float lambdaMax,
                        float lambdaCur, uint16_t sineTime);

#endif
