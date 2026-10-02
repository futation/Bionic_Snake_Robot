/**
 * AppManager.cpp - 应用业务层实现
 */

#include "AppManager.h"
#include "../motion/ManualCalib.h"
#include "ActionGroup.h"
#include "../motion/movement.h"
#include "../servo/SerialServoControl.h"

// ======================== 内部状态 ========================

static SystemMode s_mode = MODE_NORMAL;

// ======================== BinaryProtocol 发送桥接 ========================

static Bluetooth*       s_ble    = nullptr;
static HardwareSerial*   s_serial = nullptr;

static void bpSendBridge(const uint8_t* data, uint16_t len) {
    if (s_ble) {
        s_ble->sendBinaryData(data, len);
    }
}

// ======================== 模式管理 ========================

SystemMode getSystemMode() {
    return s_mode;
}

void setSystemMode(SystemMode mode) {
    s_mode = mode;
}

// ======================== 一站式初始化 ========================

void appInit(Bluetooth &ble, HardwareSerial &serial) {
    s_ble    = &ble;
    s_serial = &serial;

    // ---- 1. BLE 二进制回调绑定 ----
    Serial.println("[系统] 初始化二进制协议...");
    ble.setBinaryCallback([](uint8_t* data, uint16_t len) {
        bpParsePacket(data, len);
    });
    bpSetSendCallback(bpSendBridge);

    // ---- 2. 校准界面 handler 注册（无捕获 lambda → 函数指针） ----
    bpSetSetServoHandler([](uint8_t id, int16_t pos, int8_t offset) {
        handleManualSetServo(*s_serial, id, pos, offset);
    });
    bpSetSaveCalibHandler([](uint8_t id, int16_t pos, int8_t offset) {
        handleManualSave(*s_serial, id, pos, offset);
    });
    bpSetEnterManualCalibHandler([]() {
        enterManualCalibMode(*s_serial);
    });
    bpSetExitManualCalibHandler([]() {
        exitManualCalibMode();
    });
    bpSetChangeIDHandler([](uint8_t oldID, uint8_t newID) {
        handleChangeID(*s_serial, oldID, newID);
    });
    bpSetReadAllServosHandler([]() {
        readAllAndRespond(*s_serial);
        // bpSendFeedback(BP_FB_READ_PARAMS_OK);  
    });

    // ---- 3. 动作组界面 handler 注册 ----
    bpSetActionGroupHandler([](const uint8_t* data, uint8_t count) {
        executeActionGroup(*s_serial, data, count);
    });
    bpSetEnterActionGroupHandler([]() {
        enterActionGroupMode();
    });
    bpSetExitActionGroupHandler([]() {
        exitActionGroupMode();
    });
    bpSetScanServosHandler([]() {
        scanServoPositions(*s_serial);
    });
    bpSetPowerOffServosHandler([]() {
        powerOffServos(*s_serial);
    });

    // ---- 运动参数查询 ----
    bpSetQueryMotionLimitsHandler([]() {
        bpSendMotionLimits(
            getMaxAmplitudeDeg(),
            getMaxWaveSpeed(),
            getLambdaMin(),
            getLambdaMax(),
            getLambda(),
            getSineMoveTime()
        );
    });

    // ---- 4. 运动模块初始化 ----
    Serial.println("[系统] 初始化运动模块...");
    bool ok = movementInit(serial, &ble);
    if (!ok) {
        Serial.println("[系统] 警告：运动模块初始化失败！");
    }

    // ---- 5. 舵机上电 ----
    LobotSerialServoSetLoad(serial, LOBOT_SERVO_ID_ALL, 1);

    Serial.println("[系统] 启动完成，等待蓝牙连接...");
    Serial.println("发送 HELP 查看指令列表");
}
