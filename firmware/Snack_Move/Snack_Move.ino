/*
 * 文件名: Snack_Move.ino
 * 描述: 八节蛇运动模块测试程序
 *       上电白灯，蓝牙连接绿灯，掉电红灯
 * 硬件: ESP32, NeoPixel LED (GPIO21), 舵机总线 (UART1: RX=7, TX=8)
 */

#include "src/comm/Bluetooth.h"
#include "src/hal/RBG.h"
#include "src/motion/movement.h"
#include "src/app/AppManager.h"
#include "src/comm/StringCommands.h"

#define UART1_RX_PIN 7
#define UART1_TX_PIN 8

Bluetooth ble;

// ========== 蓝牙连接回调 ==========
void onBluetoothConnected() {
  Serial.println("=== 蓝牙已连接 ===");
  showGreen(100);
}

void onBluetoothDisconnected() {
  Serial.println("=== 蓝牙已断开 ===");
  stopGait();                    // 停止步态
  setSystemMode(MODE_NORMAL);    // 重置模式到默认
  showWhite(100);
}

// ========== 初始化 ==========
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);

  // RGB LED
  Serial.println("[系统] 初始化 LED...");
  RBG_init();
  showWhite(10);

  // 蓝牙
  Serial.println("[系统] 初始化蓝牙...");
  ble.begin();
  ble.setCommandCallback(strCmdDispatch); 
  ble.setOnConnectCallback(onBluetoothConnected);
  ble.setOnDisconnectCallback(onBluetoothDisconnected);

  // 应用层初始化
  strCmdInit(ble, Serial1);                         // 字符串命令
  appInit(ble, Serial1);                            // 二进制协议 + 运动模块 + 上电
}

void loop() {
  ble.loop();  // BLE 事件处理，永不阻塞

  // ========== 延迟步态执行 ==========
  int pending = getPendingGait();
  if (pending != GAIT_NONE) {
    clearPendingGait();
    const GaitParams& p = getGaitParams();

    showCyan(10);
    strCmdLoadAllServos();

    switch (pending) {
      case GAIT_VERT:
        sineGaitVertical(Serial1, p.ampVert, p.speedVert,
                         p.lambdaVert, p.bias, p.otherUnload);
        break;
      case GAIT_HORIZ:
        sineGaitHorizontal(Serial1, p.ampHoriz, p.speedHoriz,
                           p.lambdaHoriz, p.bias, p.otherUnload);
        break;
      case GAIT_DUAL:
        sineGaitDual(Serial1, p.ampVert, p.ampHoriz,
                     p.speedVert, p.speedHoriz,
                     p.lambdaVert, p.lambdaHoriz, p.phaseDiff);
        break;
    }

    showGreen(10);
  }

  delay(10);
}
