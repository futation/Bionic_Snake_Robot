/**
 * movement.cpp — 运动控制模块
 *
 * 基于 ServoRegistry（EEPROM 多槽位）、Calibration（校准）、
 * SerialServoControl（驱动层）封装的高层运动接口。
 */

#include "movement.h"
#include "Calibration.h"
#include "../comm/Bluetooth.h"

// ======================== 模块内部状态 ========================

static uint8_t  g_servoCount = 0;                        // 总线上舵机数量
static uint8_t  g_servoIds[MAX_REGISTERED_SERVOS];       // 舵机 ID 列表
static int16_t  g_centerPulses[MAX_REGISTERED_SERVOS];   // 中心脉冲 (按 ID 索引)
static bool     g_initialized = false;                    // 初始化标志

static volatile bool g_gaitRunning = false;               // 步态运行标志

static Bluetooth* g_ble = nullptr;                        // 蓝牙调试指针 (movementInit 时保存)

static uint16_t g_sineMoveTime = 100;     // 正弦步态单步时间 (ms)，通过 setter/getter 访问
static float    g_maxAmplitudePulse = 0;  // 系统最大允许脉冲幅度 (初始化时计算)
static float    g_maxWaveSpeed = 0;       // 系统最大波传播速度 (初始化时计算)
static float    g_lambdaMin = 0;          // lambda 推荐范围下限（基于运动组关节数）
static float    g_lambdaMax = 0;          // lambda 推荐范围上限（基于运动组关节数）

static uint8_t  g_centerSourceSlot = SLOT_CALIB;  // 当前中心基准来源槽位

// ======================== 全局可配置变量（定义） ========================
uint16_t g_defaultMoveTime = 500;  // 摆位默认时间 (ms)，外部可直接读写
float    g_lambda = 0.0f;          // 正弦波波长（关节间隔数），movementInit 自动计算为 2*(总舵机数-1)

// 蓝牙调试辅助：仅在蓝牙可用时发送
static void bleDebug(const String &msg) {
    if (g_ble) {
        g_ble->sendDataToClient(msg + "\r\n");
    }
}  

// ======================== 内部极限重算 ========================

/**
 * @brief 根据当前舵机参数重新计算所有极限值
 *
 * 计算内容：
 *   - g_maxAmplitudePulse：系统最大允许脉冲幅度（所有舵机 min(center, 1000-center)）
 *   - g_maxWaveSpeed：基于当前 lambda 和 sineMoveTime 的最大波速
 *   - g_lambdaMin / g_lambdaMax：lambda 推荐范围，基于运动组关节数 (n/2)
 *
 * 调用时机：movementInit、setLambda、setSineMoveTime 之后
 */
static void recomputeLimits() {
    // --- A_max：遍历所有舵机，取 min(center, 1000-center) ---
    g_maxAmplitudePulse = 1000.0f;
    for (uint8_t i = 0; i < g_servoCount; i++) {
        uint8_t id = g_servoIds[i];
        int16_t center = g_centerPulses[id];
        float limit = (center < 500) ? (float)center : (float)(1000 - center);
        if (limit < g_maxAmplitudePulse) {
            g_maxAmplitudePulse = limit;
        }
    }

    // --- v_max：波速 = λ × 666.667 / effectiveTime ---
    // 若 g_sineMoveTime == 0，用 50ms 兜底（舵机物理极限）
    uint16_t effectiveTime = (g_sineMoveTime == 0) ? 50 : g_sineMoveTime;
    g_maxWaveSpeed = g_lambda * 666.667f / (float)effectiveTime;

    // --- λ 推荐范围：基于运动组关节数 = g_servoCount / 2 ---
    // 公式：[(n_moving-1)/1.5,  2*(n_moving-1)]
    float n_moving = (float)g_servoCount / 2.0f;
    if (n_moving >= 2.0f) {
        g_lambdaMin = (n_moving - 1.0f) / 1.5f;
        g_lambdaMax = 2.0f * (n_moving - 1.0f);
    } else {
        g_lambdaMin = 0.0f;
        g_lambdaMax = 0.0f;
    }

    bleDebug("[Movement] Limits recomputed: A_max="
             + String(g_maxAmplitudePulse / PULSE_PER_DEGREE, 1) + "deg"
             + ", v_max=" + String(g_maxWaveSpeed, 1) + " j/s"
             + ", lambda range=[" + String(g_lambdaMin, 1)
             + ", " + String(g_lambdaMax, 1) + "]");
}

// ======================== 初始化 ========================

bool movementInit(HardwareSerial &serial, Bluetooth *ble) {
    g_ble = ble;  // 保存蓝牙指针供后续调试

    if (g_initialized) {
        bleDebug("[Movement] Already initialized, skip");
        return true;
    }

    bleDebug("[Movement] === Init start ===");

    // Step 1: 初始化舵机池
    ServoBus::init(serial);
    bleDebug("[Movement] Servo pool ready");

    // Step 2: 初始化 EEPROM 并加载校准槽
    initEEPROM();
    bool hasCalib = loadRegistry(SLOT_CALIB);

    // Step 3: 若校准槽为空 → 自动校准
    if (!hasCalib || calibRegistry.count == 0) {
        bleDebug("[Movement] Calib slot empty, auto-calibrating...");
        bleDebug("[Movement] Ensure snake is at physical center!");

        calibrateAllServos(serial, ble);

        // 重新加载校准数据
        if (!loadRegistry(SLOT_CALIB) || calibRegistry.count == 0) {
            bleDebug("[Movement] ERROR: No data after calibration!");
            return false;
        }
    }

    // Step 4: 从校准槽加载中心基准到内部数组
    if (!reloadCenterFromSlot(SLOT_CALIB, ble)) {
        bleDebug("[Movement] ERROR: Failed to load center data from calibration slot");
        return false;
    }

    // Step 5: 设置默认 lambda 并计算系统极限
    g_lambda = 2.0f * ((float)g_servoCount / 2.0f - 1.0f);  // 默认波长 = 2*(n_moving-1)，浮点除法避免截断
    recomputeLimits();

    g_initialized = true;

    String info = "[Movement] Init done - " + String(g_servoCount) + " servos ready";
    bleDebug(info);
    for (uint8_t i = 0; i < g_servoCount; i++) {
        uint8_t id = g_servoIds[i];
        String s = "  ID:" + String(id) + " Center:" + String(g_centerPulses[id]);
        bleDebug(s);
    }

    float maxAmpDeg = g_maxAmplitudePulse / PULSE_PER_DEGREE;
    bleDebug("[Movement] Max amplitude: " + String(g_maxAmplitudePulse, 1)
             + " pulses (" + String(maxAmpDeg, 1) + " deg)");
    bleDebug("[Movement] Lambda default: " + String(g_lambda, 1)
             + ", range: [" + String(g_lambdaMin, 1) + ", " + String(g_lambdaMax, 1) + "]");

    return true;
}

// ======================== 状态查询 ========================

uint8_t getServoCount() {
    return g_servoCount;
}

// ======================== 中心基准管理 ========================

bool reloadCenterFromSlot(uint8_t slot, Bluetooth *ble) {
    ServoRegistry *reg = getRegistry(slot);
    if (!reg || reg->count == 0) {
        if (ble) {
            ble->sendDataToClient("[Movement] reloadCenter: slot " + String(slot)
                                  + " (" + getSlotName(slot) + ") empty\r\n");
        }
        return false;
    }

    g_servoCount = reg->count;
    for (uint8_t i = 0; i < g_servoCount; i++) {
        uint8_t id = reg->servos[i].id;
        g_servoIds[i] = id;
        g_centerPulses[id] = reg->servos[i].position;
    }

    g_centerSourceSlot = slot;
    ServoBus::setBusInfo(g_servoIds, g_servoCount);
    recomputeLimits();

    if (ble) {
        String msg = "[Movement] Center reloaded from slot " + String(slot)
                     + " (" + getSlotName(slot) + "), "
                     + String(g_servoCount) + " servos\r\n";
        ble->sendDataToClient(msg);
    }

    Serial.println("[Movement] Center data from slot " + String(slot) + " (" + getSlotName(slot) + "):");
    for (uint8_t i = 0; i < g_servoCount; i++) {
        uint8_t id = g_servoIds[i];
        Serial.printf("  ID:%d  Center:%d\n", id, g_centerPulses[id]);
    }

    return true;
}

uint8_t getCenterSourceSlot() {
    return g_centerSourceSlot;
}

uint8_t getServoId(uint8_t index) {
    if (index < g_servoCount) {
        return g_servoIds[index];
    }
    return 0xFF;  // 无效索引
}

int16_t getCenterPulse(uint8_t servoId) {
    if (servoId < MAX_REGISTERED_SERVOS) {
        return g_centerPulses[servoId];
    }
    return -1;
}

bool isMovementReady() {
    return g_initialized;
}

// ======================== 步态参数配置 ========================

void setSineMoveTime(uint16_t time_ms) {
    if (time_ms != g_sineMoveTime) {
        g_sineMoveTime = time_ms;
        // 联动更新最大波速（使用当前全局 lambda）
        recomputeLimits();
        bleDebug("[Movement] Sine move time set to " + String(time_ms) + " ms"
                 + (time_ms == 0 ? " (0=use 50ms physical limit)" : "")
                 + ", max speed=" + String(g_maxWaveSpeed, 1));
    }
}

uint16_t getSineMoveTime() {
    return g_sineMoveTime;
}

float getMaxAmplitudeDeg() {
    return g_maxAmplitudePulse / PULSE_PER_DEGREE;
}

float getMaxWaveSpeed() {
    // 舵机物理极限：最坏情况从脉冲 0→1000（转 240°）在 g_sineMoveTime ms 内完成
    // ω_max = 240° / (g_sineMoveTime/1000) = 240000 / g_sineMoveTime  (deg/s)
    // v_per_joint = ω_max / 360 = 666.67 / g_sineMoveTime  (关节/秒, λ=1)
    return g_maxWaveSpeed;
}

void setLambda(float lambda) {
    if (lambda > 0.0f && lambda != g_lambda) {
        g_lambda = lambda;
        recomputeLimits();
        bleDebug("[Movement] Lambda set to " + String(g_lambda, 1) + ", max speed=" + String(g_maxWaveSpeed, 1));
    }
}

float getLambda() {
    return g_lambda;
}

float getLambdaMin() {
    return g_lambdaMin;
}

float getLambdaMax() {
    return g_lambdaMax;
}



// ======================== 摆位函数 ========================

void resetToCenter(HardwareSerial &serial) {
    if (!g_initialized) {
        bleDebug("[Movement] ERROR: Not initialized, cannot reset");
        return;
    }

    bleDebug("[Movement] Reset to center - all servos");

    for (uint8_t i = 0; i < g_servoCount; i++) {
        uint8_t id = g_servoIds[i];
        SingleServoManager *servo = ServoBus::get(id);
        if (servo != nullptr) {
            servo->moveImmediate(g_centerPulses[id], g_defaultMoveTime);
        }
    }

    bleDebug("[Movement] Reset done");
}

void moveToPose(HardwareSerial &serial, uint8_t slot) {
    if (!g_initialized) {
        bleDebug("[Movement] ERROR: Not initialized, cannot move to pose");
        return;
    }

    // 校验槽位
    if (slot != SLOT_SCAN1 && slot != SLOT_SCAN2) {
        bleDebug("[Movement] ERROR: moveToPose only accepts SLOT_SCAN1 or SLOT_SCAN2");
        return;
    }

    // 从 EEPROM 重新加载，确保数据最新
    if (!loadRegistry(slot)) {
        bleDebug("[Movement] Slot " + String(slot) + " (" + getSlotName(slot) + ") no valid data");
        return;
    }

    ServoRegistry *reg = getRegistry(slot);
    if (reg == nullptr || reg->count == 0) {
        bleDebug("[Movement] Slot " + String(slot) + " empty, pose failed");
        return;
    }

    bleDebug("[Movement] Move to pose - slot " + String(slot) + " (" + getSlotName(slot) + ") " + String(reg->count) + " servos");

    for (uint8_t i = 0; i < reg->count; i++) {
        uint8_t  id  = reg->servos[i].id;
        int16_t  pos = reg->servos[i].position;
        SingleServoManager *servo = ServoBus::get(id);
        if (servo != nullptr) {
            servo->moveImmediate(pos, g_defaultMoveTime);
        }
    }

    bleDebug("[Movement] Pose done");
}

// ======================== 步态控制 ========================

void stopGait() {
    g_gaitRunning = false;
    bleDebug("[Movement] Gait stop signal sent");
}

// ======================== 正弦步态（奇数/偶数舵机组） ========================

/**
 * @brief 内部辅助：对一组舵机执行正弦步态循环
 * @param groupIds   舵机 ID 数组（调用方预先按奇偶/需求筛选）
 * @param groupCount 组内舵机数量
 * @param lambda     波长（关节间隔数），外部已处理默认值
 * @param A_p        脉冲幅度（未钳位，循环内钳位到 [0,1000]）
 * @param wave_speed 波传播速度（关节间隔/秒）
 * @param bias_pulse 偏置脉冲（有符号，转弯用）
 */
static void runSineGroupLoop(const uint8_t groupIds[], uint8_t groupCount,
                              float lambda, float A_p, float wave_speed, int16_t bias_pulse) {
    const float wavelength = lambda;
    float t = 0.0f;

    while (g_gaitRunning) {
        unsigned long cycleStart = millis();

        for (uint8_t j = 0; j < groupCount; j++) {
            uint8_t id = groupIds[j];
            int16_t center = g_centerPulses[id];

            float phase = (2.0f * PI / wavelength)
                          * ((float)j + wave_speed * t);
            float pulse_float = (float)(center + bias_pulse) + A_p * sin(phase);

            if (pulse_float > 1000.0f) pulse_float = 1000.0f;
            if (pulse_float < 0.0f)   pulse_float = 0.0f;

            SingleServoManager *servo = ServoBus::get(id);
            if (servo != nullptr) {
                servo->moveImmediate((uint16_t)(pulse_float + 0.5f), g_sineMoveTime);
            }
        }

        delay(g_sineMoveTime);
        unsigned long cycleEnd = millis();
        t += (cycleEnd - cycleStart) / 1000.0f;
    }
}

void sineGaitVertical(HardwareSerial &serial, float amplitude_deg, float wave_speed,
                      float lambda,int16_t bias_deg, bool otherUnload) {                     
    if (!g_initialized) {
        bleDebug("[Movement] ERROR: Not initialized, cannot run gait");
        return;
    }

    // lambda 默认值处理
    float actualLambda = (lambda > 0.0f) ? lambda : g_lambda;

    // 预收集偶数 ID 舵机
    uint8_t groupIds[MAX_REGISTERED_SERVOS];
    uint8_t groupCount = 0;
    for (uint8_t i = 0; i < g_servoCount; i++) {
        if (g_servoIds[i] % 2 == 0) {
            groupIds[groupCount++] = g_servoIds[i];
        }
    }

    if (groupCount == 0) {
        bleDebug("[Movement] WARNING: No even-ID servos for vertical gait");
        return;
    }

    // 振幅：由用户自行控制，脉冲层面在循环内钳位到 [0,1000]
    float A_p = amplitude_deg * PULSE_PER_DEGREE;

    // 偏置角 → 脉冲
    int16_t bias_pulse = (int16_t)((float)bias_deg * PULSE_PER_DEGREE);

    bleDebug("[Movement] Vertical sine gait - lambda=" + String(actualLambda, 1)
             + ", " + String(groupCount) + " servos, amp=" + String(amplitude_deg, 1)
             + "deg, speed=" + String(wave_speed, 1)
             + ", bias=" + String(bias_deg) + "deg"
             + (otherUnload ? ", odd-group unloaded" : ""));

    // 运动前：奇数 ID 组掉电
    if (otherUnload) {
        for (uint8_t i = 0; i < g_servoCount; i++) {
            if (g_servoIds[i] % 2 == 1) {
                LobotSerialServoSetLoad(serial, g_servoIds[i], 0);
            }
        }
    }

    g_gaitRunning = true;
    runSineGroupLoop(groupIds, groupCount, actualLambda, A_p, wave_speed, bias_pulse);
    g_gaitRunning = false;

    // 运动后：奇数 ID 组恢复上电
    if (otherUnload) {
        for (uint8_t i = 0; i < g_servoCount; i++) {
            if (g_servoIds[i] % 2 == 1) {
                LobotSerialServoSetLoad(serial, g_servoIds[i], 1);
            }
        }
    }

    bleDebug("[Movement] Vertical sine gait ended");
}

void sineGaitHorizontal(HardwareSerial &serial, float amplitude_deg, float wave_speed,
                        float lambda,int16_t bias_deg, bool otherUnload) {
    if (!g_initialized) {
        bleDebug("[Movement] ERROR: Not initialized, cannot run gait");
        return;
    }

    // lambda 默认值处理
    float actualLambda = (lambda > 0.0f) ? lambda : g_lambda;

    // 预收集奇数 ID 舵机
    uint8_t groupIds[MAX_REGISTERED_SERVOS];
    uint8_t groupCount = 0;
    for (uint8_t i = 0; i < g_servoCount; i++) {
        if (g_servoIds[i] % 2 == 1) {
            groupIds[groupCount++] = g_servoIds[i];
        }
    }

    if (groupCount == 0) {
        bleDebug("[Movement] WARNING: No odd-ID servos for horizontal gait");
        return;
    }

    // 振幅：由用户自行控制，脉冲层面在循环内钳位到 [0,1000]
    float A_p = amplitude_deg * PULSE_PER_DEGREE;

    // 偏置角 → 脉冲
    int16_t bias_pulse = (int16_t)((float)bias_deg * PULSE_PER_DEGREE);

    bleDebug("[Movement] Horizontal sine gait - lambda=" + String(actualLambda, 1)
             + ", " + String(groupCount) + " servos, amp=" + String(amplitude_deg, 1)
             + "deg, speed=" + String(wave_speed, 1)
             + ", bias=" + String(bias_deg) + "deg"
             + (otherUnload ? ", even-group unloaded" : ""));

    // 运动前：偶数 ID 组掉电
    if (otherUnload) {
        for (uint8_t i = 0; i < g_servoCount; i++) {
            if (g_servoIds[i] % 2 == 0) {
                LobotSerialServoSetLoad(serial, g_servoIds[i], 0);
            }
        }
    }

    g_gaitRunning = true;
    runSineGroupLoop(groupIds, groupCount, actualLambda, A_p, wave_speed, bias_pulse);
    g_gaitRunning = false;

    // 运动后：偶数 ID 组恢复上电
    if (otherUnload) {
        for (uint8_t i = 0; i < g_servoCount; i++) {
            if (g_servoIds[i] % 2 == 0) {
                LobotSerialServoSetLoad(serial, g_servoIds[i], 1);
            }
        }
    }

    bleDebug("[Movement] Horizontal sine gait ended");
}

// ======================== 双波合成步态 ========================

void sineGaitDual(HardwareSerial &serial,
                  float amplitude_vert, float amplitude_horiz,
                  float wave_speed_vert, float wave_speed_horiz,
                  float lambda_vert, float lambda_horiz,
                  float phase_diff) {
    if (!g_initialized) {
        bleDebug("[Movement] ERROR: Not initialized, cannot run dual gait");
        return;
    }

    // lambda 默认值处理
    float actualLambdaV = (lambda_vert > 0.0f) ? lambda_vert : g_lambda;
    float actualLambdaH = (lambda_horiz > 0.0f) ? lambda_horiz : g_lambda;

    // 预收集奇偶两组 ID
    uint8_t evenIds[MAX_REGISTERED_SERVOS];
    uint8_t oddIds[MAX_REGISTERED_SERVOS];
    uint8_t evenCount = 0, oddCount = 0;
    for (uint8_t i = 0; i < g_servoCount; i++) {
        uint8_t id = g_servoIds[i];
        if (id % 2 == 0) {
            evenIds[evenCount++] = id;
        } else {
            oddIds[oddCount++] = id;
        }
    }

    if (oddCount == 0 || evenCount == 0) {
        bleDebug("[Movement] WARNING: Need both odd and even servos for dual gait");
        return;
    }

    // 振幅：由用户自行控制，脉冲层面在循环内钳位到 [0,1000]
    float A_p_vert = amplitude_vert * PULSE_PER_DEGREE;
    float A_p_horiz = amplitude_horiz * PULSE_PER_DEGREE;

    bleDebug("[Movement] Dual sine gait - lambdaV=" + String(actualLambdaV, 1)
             + " lambdaH=" + String(actualLambdaH, 1)
             + ", vert(even)=" + String(evenCount)
             + " amp=" + String(amplitude_vert, 1) + "deg speed=" + String(wave_speed_vert, 1)
             + ", horiz(odd)=" + String(oddCount)
             + " amp=" + String(amplitude_horiz, 1) + "deg speed=" + String(wave_speed_horiz, 1)
             + ", phase_diff=" + String(phase_diff, 3) + "rad");

    g_gaitRunning = true;
    float t = 0.0f;

    while (g_gaitRunning) {
        unsigned long cycleStart = millis();

        // 偶数 → 垂直波
        for (uint8_t j = 0; j < evenCount; j++) {
            uint8_t id = evenIds[j];
            int16_t center = g_centerPulses[id];
            float phase = (2.0f * PI / actualLambdaV) * ((float)j + wave_speed_vert * t);
            float pulse_float = (float)center + A_p_vert * sin(phase);

            if (pulse_float > 1000.0f) pulse_float = 1000.0f;
            if (pulse_float < 0.0f)   pulse_float = 0.0f;

            SingleServoManager *servo = ServoBus::get(id);
            if (servo != nullptr) {
                servo->moveImmediate((uint16_t)(pulse_float + 0.5f), g_sineMoveTime);
            }
        }

        // 奇数 → 水平波 + 相位差
        for (uint8_t j = 0; j < oddCount; j++) {
            uint8_t id = oddIds[j];
            int16_t center = g_centerPulses[id];
            float phase = (2.0f * PI / actualLambdaH) * ((float)j + wave_speed_horiz * t)
                          + phase_diff;
            float pulse_float = (float)center + A_p_horiz * sin(phase);

            if (pulse_float > 1000.0f) pulse_float = 1000.0f;
            if (pulse_float < 0.0f)   pulse_float = 0.0f;

            SingleServoManager *servo = ServoBus::get(id);
            if (servo != nullptr) {
                servo->moveImmediate((uint16_t)(pulse_float + 0.5f), g_sineMoveTime);
            }
        }

        delay(g_sineMoveTime);
        unsigned long cycleEnd = millis();
        t += (cycleEnd - cycleStart) / 1000.0f;
    }

    g_gaitRunning = false;
    bleDebug("[Movement] Dual sine gait ended");
}