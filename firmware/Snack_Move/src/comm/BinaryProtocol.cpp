/**
 * BinaryProtocol.cpp - 二进制通信协议实现 v2.0
 */

#include "BinaryProtocol.h"

// ======================== 内部状态 ========================

static BPCallback_SendData      s_sendFn             = nullptr;

static BPCallback_SetServo      s_onSetServo         = nullptr;
static BPCallback_SetServo      s_onSaveCalib        = nullptr;
static BPCallback_ActionGroup   s_onActionGroup      = nullptr;
static BPCallback_Void          s_onEnterManualCalib = nullptr;
static BPCallback_Void          s_onExitManualCalib  = nullptr;
static BPCallback_ChangeID      s_onChangeID         = nullptr;
static BPCallback_Void          s_onReadAllServos    = nullptr;
static BPCallback_Void          s_onEnterActionGroup = nullptr;
static BPCallback_Void          s_onExitActionGroup  = nullptr;
static BPCallback_Void          s_onScanServos       = nullptr;
static BPCallback_Void          s_onPowerOffServos   = nullptr;
static BPCallback_Void          s_onQueryMotionLimits = nullptr;

// ======================== 注册函数 ========================

void bpSetSendCallback(BPCallback_SendData sendFn) { s_sendFn = sendFn; }

void bpSetSetServoHandler(BPCallback_SetServo cb)          { s_onSetServo = cb; }
void bpSetSaveCalibHandler(BPCallback_SetServo cb)         { s_onSaveCalib = cb; }
void bpSetActionGroupHandler(BPCallback_ActionGroup cb)    { s_onActionGroup = cb; }
void bpSetEnterManualCalibHandler(BPCallback_Void cb)      { s_onEnterManualCalib = cb; }
void bpSetExitManualCalibHandler(BPCallback_Void cb)       { s_onExitManualCalib = cb; }
void bpSetChangeIDHandler(BPCallback_ChangeID cb)          { s_onChangeID = cb; }
void bpSetReadAllServosHandler(BPCallback_Void cb)         { s_onReadAllServos = cb; }
void bpSetEnterActionGroupHandler(BPCallback_Void cb)      { s_onEnterActionGroup = cb; }
void bpSetExitActionGroupHandler(BPCallback_Void cb)       { s_onExitActionGroup = cb; }
void bpSetScanServosHandler(BPCallback_Void cb)            { s_onScanServos = cb; }
void bpSetPowerOffServosHandler(BPCallback_Void cb)        { s_onPowerOffServos = cb; }
void bpSetQueryMotionLimitsHandler(BPCallback_Void cb)     { s_onQueryMotionLimits = cb; }

// ======================== 校验和 ========================

uint8_t bpCalcChecksum(uint8_t cmd, uint8_t len, const uint8_t* payload) {
    uint16_t sum = cmd + len;
    for (uint8_t i = 0; i < len; i++) {
        sum += payload[i];
    }
    return 0xFF - (sum & 0xFF);
}

// ======================== 构建帧 ========================

uint16_t bpBuildPacket(uint8_t cmd, const uint8_t* payload, uint8_t len, uint8_t* outBuf) {
    outBuf[0] = BP_HEADER;
    outBuf[1] = cmd;
    outBuf[2] = len;
    if (len > 0 && payload != nullptr) {
        memcpy(outBuf + 3, payload, len);
    }
    outBuf[3 + len] = bpCalcChecksum(cmd, len, payload);
    outBuf[4 + len] = BP_FOOTER;
    return 5 + len;  // 总帧长 = 帧头(1) + Cmd(1) + Len(1) + Payload(len) + CS(1) + 帧尾(1)
}

// ======================== 发送辅助 ========================

static void bpSend(const uint8_t* data, uint16_t len) {
    if (s_sendFn) {
        s_sendFn(data, len);
    }
}

// ======================== 反馈发送 ========================

void bpSendFeedback(uint8_t statusCode) {
    uint8_t buf[16];
    uint16_t total = bpBuildPacket(BP_CMD_FEEDBACK, &statusCode, 1, buf);
    bpSend(buf, total);
}

void bpSendFeedbackFail(uint8_t reasonCode) {
    uint8_t payload[2];
    payload[0] = BP_FB_GENERAL_FAIL;
    payload[1] = reasonCode;
    uint8_t buf[16];
    uint16_t total = bpBuildPacket(BP_CMD_FEEDBACK, payload, 2, buf);
    bpSend(buf, total);
}

// ======================== 上报响应发送 ========================

void bpSendReportAllServos(uint8_t count, const uint8_t* ids,
                           const int16_t* positions, const int8_t* offsets) {
    if (count == 0) {
        // 发送空包（总线上无舵机）
        uint8_t buf[8];
        uint16_t total = bpBuildPacket(BP_CMD_REPORT_ALL_SERVOS, nullptr, 0, buf);
        bpSend(buf, total);
        return;
    }

    // Payload: [ID(1) Pos_L(1) Pos_H(1) Offset(1)] × count（不含 Count 前缀）
    uint8_t payloadLen = count * 4;
    if (payloadLen > BP_MAX_PAYLOAD) return;

    uint8_t payload[BP_MAX_PAYLOAD];
    for (uint8_t i = 0; i < count; i++) {
        uint8_t base = i * 4;
        payload[base + 0] = ids[i];
        payload[base + 1] = (uint8_t)(positions[i] & 0xFF);         // Pos 低字节
        payload[base + 2] = (uint8_t)((positions[i] >> 8) & 0xFF);  // Pos 高字节
        payload[base + 3] = (uint8_t)(offsets[i] & 0xFF);           // Offset（有符号→无符号）
    } 

    uint8_t buf[BP_MAX_PAYLOAD + 8];
    uint16_t total = bpBuildPacket(BP_CMD_REPORT_ALL_SERVOS, payload, payloadLen, buf);
    bpSend(buf, total);
}

void bpSendScanResult(uint8_t count, const uint8_t* ids, const int16_t* positions) {
    if (count == 0) {
        // 发送空包（总线上无舵机可扫描）
        uint8_t buf[8];
        uint16_t total = bpBuildPacket(BP_CMD_REPORT_SCAN_RESULT, nullptr, 0, buf);
        bpSend(buf, total);
        return;
    }

    // Payload: [ID(1) Pos_L(1) Pos_H(1)] × count
    uint8_t payloadLen = count * 3;
    if (payloadLen > BP_MAX_PAYLOAD) return;

    uint8_t payload[BP_MAX_PAYLOAD];
    for (uint8_t i = 0; i < count; i++) {
        uint8_t base = i * 3;
        payload[base + 0] = ids[i];
        payload[base + 1] = (uint8_t)(positions[i] & 0xFF);         // Pos 低字节
        payload[base + 2] = (uint8_t)((positions[i] >> 8) & 0xFF);  // Pos 高字节
    }

    uint8_t buf[BP_MAX_PAYLOAD + 8];
    uint16_t total = bpBuildPacket(BP_CMD_REPORT_SCAN_RESULT, payload, payloadLen, buf);
    bpSend(buf, total);
}

// ======================== 运动极限上报 ========================

void bpSendMotionLimits(float aMax, float vMax, float lambdaMin, float lambdaMax,
                        float lambdaCur, uint16_t sineTime) {
    uint8_t payload[22];
    memcpy(payload + 0,  &aMax,       4);
    memcpy(payload + 4,  &vMax,       4);
    memcpy(payload + 8,  &lambdaMin,  4);
    memcpy(payload + 12, &lambdaMax,  4);
    memcpy(payload + 16, &lambdaCur,  4);
    payload[20] = (uint8_t)(sineTime & 0xFF);         // Time 低字节
    payload[21] = (uint8_t)((sineTime >> 8) & 0xFF);  // Time 高字节

    uint8_t buf[BP_MAX_PAYLOAD + 8];
    uint16_t total = bpBuildPacket(BP_CMD_REPORT_MOTION_LIMITS, payload, 22, buf);
    bpSend(buf, total);
}

// ======================== 帧解析与分发 ========================

void bpParsePacket(const uint8_t* data, uint16_t len) {
    // 最小帧长：帧头(1) + Cmd(1) + Len(1) + CS(1) + 帧尾(1) = 5
    if (len < 5) return;
    if (data[0] != BP_HEADER) return;
    if (data[len - 1] != BP_FOOTER) return;

    uint8_t cmd = data[1];
    uint8_t payloadLen = data[2];

    // 验证长度一致性
    if (len != 5 + payloadLen) {
        bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
        return;
    }

    const uint8_t* payload = (payloadLen > 0) ? (data + 3) : nullptr;

    // 验证校验和
    uint8_t expectedCS = bpCalcChecksum(cmd, payloadLen, payload);
    uint8_t receivedCS  = data[3 + payloadLen];
    if (expectedCS != receivedCS) {
        bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
        return;
    }

    // ======================== 命令分发 ========================
    // 每个分支：先验证 Payload 长度 → 再检查 handler → 调用
    // 格式错误 → BP_ERR_CHECKSUM_FORMAT
    // handler 未注册 → BP_ERR_MODE_MISMATCH

    switch (cmd) {

        // ---------- 校准界面 H→M ----------

        case BP_CMD_SET_SERVO:   // 0x01
            // Payload: ID(1) Pos_L(1) Pos_H(1) Offset(1), Len=4
            if (payloadLen != 4) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onSetServo) {
                uint8_t  id     = payload[0];
                int16_t  pos    = (int16_t)((uint16_t)payload[1] | ((uint16_t)payload[2] << 8));
                int8_t   offset = (int8_t)payload[3];
                s_onSetServo(id, pos, offset);
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_SAVE_CALIB:  // 0x03
            // Payload: ID(1) Pos_L(1) Pos_H(1) Offset(1), Len=4
            if (payloadLen != 4) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onSaveCalib) {
                uint8_t  id     = payload[0];
                int16_t  pos    = (int16_t)((uint16_t)payload[1] | ((uint16_t)payload[2] << 8));
                int8_t   offset = (int8_t)payload[3];
                s_onSaveCalib(id, pos, offset);
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_ENTER_MANUAL_CALIB:  // 0x05, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onEnterManualCalib) {
                s_onEnterManualCalib();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_CHANGE_ID:   // 0x07
            // Payload: oldID(1) newID(1), Len=2
            if (payloadLen != 2) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onChangeID) {
                s_onChangeID(payload[0], payload[1]);
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_READ_ALL_SERVOS:  // 0x08, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onReadAllServos) {
                s_onReadAllServos();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_EXIT_MANUAL_CALIB:  // 0x09, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onExitManualCalib) {
                s_onExitManualCalib();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        // ---------- 动作组界面 H→M ----------

        case BP_CMD_ACTION_GROUP:  // 0x02
            // Payload: [ID(1) Pos(2) Time(2)]×N, Len = 5×N
            if (payloadLen < 5 || (payloadLen % 5) != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onActionGroup) {
                s_onActionGroup(payload, payloadLen / 5);
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_ENTER_ACTION_GROUP:  // 0x0A, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onEnterActionGroup) {
                s_onEnterActionGroup();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_EXIT_ACTION_GROUP:  // 0x0B, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onExitActionGroup) {
                s_onExitActionGroup();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_SCAN_SERVOS:  // 0x0C, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onScanServos) {
                s_onScanServos();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_POWER_OFF_SERVOS:  // 0x0E, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onPowerOffServos) {
                s_onPowerOffServos();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        case BP_CMD_QUERY_MOTION_LIMITS:  // 0x0F, Len=0
            if (payloadLen != 0) {
                bpSendFeedbackFail(BP_ERR_CHECKSUM_FORMAT);
                break;
            }
            if (s_onQueryMotionLimits) {
                s_onQueryMotionLimits();
            } else {
                bpSendFeedbackFail(BP_ERR_MODE_MISMATCH);
            }
            break;

        // ---------- M→H 命令（MCU 负责发送，不应接收） ----------
        case BP_CMD_FEEDBACK:            // 0x04
        case BP_CMD_REPORT_ALL_SERVOS:   // 0x06
        case BP_CMD_REPORT_SCAN_RESULT:  // 0x0D
        case BP_CMD_REPORT_MOTION_LIMITS: // 0x10
            // 静默忽略（上位机误发或回显）
            break;

        default:
            bpSendFeedbackFail(BP_ERR_INVALID_PARAM);
            break;
    }
}
