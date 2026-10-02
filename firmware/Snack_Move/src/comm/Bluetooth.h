#ifndef BLUETOOTH_H
#define BLUETOOTH_H

/**
 * @file Bluetooth.h
 * @brief BLE蓝牙通信模块头文件
 * 
 * 本模块封装了ESP32的BLE功能，提供设备广播、连接管理、命令接收和数据发送等核心功能
 * 与RGB灯光控制模块协同工作，实现通过手机APP远程控制LED灯效
 * 
 * 主要特性：
 * - 完整的BLE服务器实现
 * - 命令接收和数据发送双通道
 * - 自动连接管理和重连机制
 * - 与RGB模块的无缝集成
 */

#include <BLEDevice.h>      // BLE设备核心库
#include <BLEServer.h>      // BLE服务器功能
#include <BLEUtils.h>       // BLE工具函数
#include <BLE2902.h>        // BLE描述符支持
#include "../hal/RBG.h"            // NeoPixel灯光控制库

/**
 * @brief BLE蓝牙通信主类
 * 
 * 封装了整个BLE通信栈，提供从设备初始化到数据交互的完整解决方案
 * 采用观察者模式处理客户端连接事件和命令接收
 * 
 * 使用示例：
 * @code
 * Bluetooth ble;           // 创建蓝牙对象
 * ble.begin();             // 初始化蓝牙
 * ble.setDeviceName("MyESP32"); // 设置设备名称
 * 
 * void loop() 
 * {
 *   ble.loop();            // 蓝牙主循环
 *   if(ble.isConnected()) {
 *     // 连接状态下的特殊处理
 *   }
 * }
 * @endcode
 */
class Bluetooth 
{
  public:
    /**
      * @brief 默认构造函数
      * 初始化内部成员变量，创建蓝牙对象实例
      */
    Bluetooth();
    
    /**
      * @brief 蓝牙模块初始化函数
      * 完成BLE设备栈初始化、服务创建、特征设置和广播启动
      * 必须在setup()函数中调用，且只调用一次
      * 
      * @note 初始化顺序：设备→服务器→服务→特征→广播
      */
    void begin();
    
    /**
      * @brief 蓝牙主循环处理函数
      * 处理需要周期性执行的后台任务，如状态监测、数据发送等
      * 应在Arduino的loop()函数中持续调用
      */
    void loop();
      
    /**
      * @brief 向已连接的BLE客户端发送数据
      * 通过通知机制主动推送数据到订阅的客户端
      * 
      * @param data 要发送的字符串数据，支持任意文本内容
      * @return void
      * 
      * @note 数据长度受BLE MTU限制，建议单次发送不超过20字节
      */
    void sendDataToClient(String data);
    
    /**
      * @brief 设置BLE设备广播名称
      * 修改设备在客户端扫描列表中显示的名称
      * 
      * @param name 设备名称字符串，以空字符结尾
      * 
      * @note 必须在begin()调用前设置才能生效
      */
    void setDeviceName(const char* name);
    
    /**
      * @brief 获取当前BLE连接状态
      * 用于判断是否可以进行数据通信
      * 
      * @return bool true=设备已连接，false=设备未连接
      */
    bool isConnected();

    void onCommandReceived(String command);

    // 设置命令处理回调的函数
    void setCommandCallback(void (*callback)(String command));
    // 设置二进制数据回调的函数
    void setBinaryCallback(void (*callback)(uint8_t* data, uint16_t len));
    // 设置主循环回调的函数
    void setLoopCallback(void (*callback)(void));
    // 设置连接事件回调的函数
    void setOnConnectCallback(void (*callback)(void));
    // 设置断开连接事件回调的函数
    void setOnDisconnectCallback(void (*callback)(void));

    // 发送二进制数据
    void sendBinaryData(const uint8_t* data, uint16_t len);

  private:
    // ============================ BLE核心组件指针 ============================
    BLEServer *pServer;                     ///< BLE服务器实例指针，管理连接和广播
    BLECharacteristic *pCommandCharacteristic; ///< 命令接收特征，处理客户端控制指令
    BLECharacteristic *pDataCharacteristic;   ///< 数据发送特征，向客户端推送状态信息
    bool deviceConnected;                   ///< 连接状态标志，true表示有客户端连接

    // ============================ BLE UUID配置 ============================
    /// 主服务UUID，唯一标识蓝牙服务，使用随机生成的128位UUID
    const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    
    /// 命令特征UUID，客户端通过此特征发送控制命令
    const char* COMMAND_CHARACTERISTIC_UUID = "1b9a473a-4493-4536-8b2b-9d4133488256";
    
    /// 数据特征UUID，服务器通过此特征向客户端发送数据
    const char* DATA_CHARACTERISTIC_UUID = "2b9a473a-4493-4536-8b2b-9d4133488257";
    
    // ============================ 设备配置 ============================
    const char* deviceName = "Robotic-Snack"; ///< 默认设备名称，客户端扫描时可见

    // 回调函数指针
    void (*_userCommandCallback)(String) = nullptr;
    void (*_userBinaryCallback)(uint8_t* data, uint16_t len) = nullptr;
    void (*_userLoopCallback)(void) = nullptr;
    void (*_userOnConnectCallback)(void) = nullptr;
    void (*_userOnDisconnectCallback)(void) = nullptr;

    // 二进制帧接收缓冲区
    static const uint16_t BINARY_BUF_SIZE = 256;
    uint8_t  _binaryBuf[256];
    uint16_t _binaryBufLen = 0;
    
    // ============================ 内部回调类定义 ============================

    /**
     * @brief BLE服务器连接事件回调类
     * 
     * 处理客户端的连接和断开事件，维护连接状态标志
     * 采用父子对象设计模式，通过父指针访问外部类成员
     */
    class MyServerCallbacks : public BLEServerCallbacks 
    {
      private:
          Bluetooth* parent; ///< 指向外部Bluetooth对象的指针，用于访问成员变量和方法
          
      public:
          /**
           * @brief 构造函数，建立与父对象的关联
           * @param p 外部Bluetooth对象的指针
           */
          MyServerCallbacks(Bluetooth* p) : parent(p) {}
          
          /**
           * @brief 客户端连接成功回调
           * @param pServer 触发事件的服务器对象
           */
          void onConnect(BLEServer* pServer) 
          {
            parent->deviceConnected = true;  // 更新连接状态
            Serial.println("Device Connected");
            // 发送欢迎消息和可用命令说明
            parent->sendDataToClient("ESP32 Connected!\r\n");

            // 调用用户设置的连接回调
            if (parent->_userOnConnectCallback != nullptr) 
            {
                parent->_userOnConnectCallback();
            }
          }

          /**
           * @brief 客户端断开连接回调
           * @param pServer 触发事件的服务器对象
           */
          void onDisconnect(BLEServer* pServer) 
          {
              parent->deviceConnected = false; // 清除连接状态
              Serial.println("Device Disconnected, restarting advertising...");
              // 立即重新开始广播，允许新设备连接
              pServer->getAdvertising()->start();

            // 调用用户设置的断开回调
            if (parent->_userOnDisconnectCallback != nullptr) 
            {
                parent->_userOnDisconnectCallback();
            }
          }
    };

    /**
     * @brief BLE命令特征写入事件回调类
     * 
     * 处理客户端向命令特征写入的数据，解析并转发给命令处理函数
     */
    class CommandCallbacks : public BLECharacteristicCallbacks 
    {
      private:
          Bluetooth* parent; ///< 指向外部Bluetooth对象的指针
          
      public:
        /**
        * @brief 构造函数，建立与父对象的关联
        * @param p 外部Bluetooth对象的指针
        */
        CommandCallbacks(Bluetooth* p) : parent(p) {}
          
        /**
        * @brief 特征值写入事件处理
        * @param pCharacteristic 被写入的特征对象
        */
        void onWrite(BLECharacteristic* pCharacteristic) 
        {
          size_t len = pCharacteristic->getLength();
          if (len == 0 || len > BINARY_BUF_SIZE) return;

          uint8_t* rawData = pCharacteristic->getData();
          if (rawData == nullptr) return;

          // ========== 二进制帧检测 (首字节 0xAA) ==========
          // 如果缓冲区已有残留数据，说明上一次写入的二进制帧还没收完，
          // 本次数据无条件追加到缓冲区，不按首字节分流
          if (parent->_binaryBufLen > 0 || rawData[0] == 0xAA) 
          {
            // 累积到缓冲区
            for (size_t i = 0; i < len && parent->_binaryBufLen < BINARY_BUF_SIZE; i++) 
            {
                parent->_binaryBuf[parent->_binaryBufLen++] = rawData[i];
            }

            // 尝试解析完整帧
            while (parent->_binaryBufLen >= 5) 
            {
              // 找帧头
              if (parent->_binaryBuf[0] != 0xAA) 
              {
                // 首字节不是帧头，丢弃一个字节继续搜索
                memmove(parent->_binaryBuf, parent->_binaryBuf + 1, --parent->_binaryBufLen);
                continue;
              }

              uint8_t payloadLen = parent->_binaryBuf[2]; //包数据长度
              uint16_t frameLen = 5 + payloadLen;         //包总长

              // 长度合法性检查
              if (frameLen > BINARY_BUF_SIZE || frameLen < 5) {
                // 非法长度，丢弃当前帧头，继续搜索
                memmove(parent->_binaryBuf, parent->_binaryBuf + 1, --parent->_binaryBufLen);
                continue;
              }

              //已收齐整个帧
              if (parent->_binaryBufLen >= frameLen)
              {
                if (parent->_binaryBuf[frameLen - 1] == 0x55) {
                  // 完整帧 → 调用二进制回调
                  if (parent->_userBinaryCallback) {
                      parent->_userBinaryCallback(parent->_binaryBuf, frameLen);
                  }
                }
                // 移除已处理的帧（无论正确或错误）
                uint16_t remaining = parent->_binaryBufLen - frameLen;
                if (remaining > 0) {
                    memmove(parent->_binaryBuf, parent->_binaryBuf + frameLen, remaining);
                }
                parent->_binaryBufLen = remaining;
              } 
              else {
                  break;  // 等待更多数据
              }
            }
            //若缓冲区仍有数据但首字节不是0xAA，说明混入了非二进制数据，清空缓冲区以恢复状态
            if (parent->_binaryBufLen > 0 && parent->_binaryBuf[0] != 0xAA) 
            {
              parent->_binaryBufLen = 0;
            }
            return;
          }
          // ========== 字符串命令处理 ==========
          else if (len >= 4 && memcmp(rawData, "CMD_", 4) == 0) 
          {
            String value = pCharacteristic->getValue();                        
            // 提取 "CMD_" 后面的部分
            String commandStr = value.substring(4);
            
            if (commandStr.length() > 0) 
            {
                // 发送确认消息
                String ackMsg = "CMD received: ";
                ackMsg += commandStr;
                ackMsg += "\r\n";
                parent->sendDataToClient(ackMsg);
                Serial.println(ackMsg);
                
                //转向回调处理函数
                parent->onCommandReceived(commandStr);
            }
            else 
            {
                // "CMD_" 后面没有内容
                Serial.println("Error: No command after CMD_");
                parent->sendDataToClient("Error: No command after CMD_\r\n");
            }
            return;
          }
          else 
          {
            Serial.print("Unknown data received (");
            Serial.print(len);
            Serial.print(" bytes): ");
            delay(1);
            // 1. 串口打印原始十六进制数据
            for (size_t i = 0; i < len; i++) {
                Serial.printf("%02X ", rawData[i]);
            }
            Serial.println();
            delay(1);
            // 2. 串口打印 String 格式（不可打印字符用 '.' 代替）
            Serial.print("String: ");
            for (size_t i = 0; i < len; i++) {
                if (isprint(rawData[i])) {
                    Serial.print((char)rawData[i]);
                } else {
                    Serial.print('.');
                }
            }
            Serial.println();
            delay(1);
            // 可选：发送错误信息给客户端
            // 3. 蓝牙回传：用 0xFF 包裹原始数据
            //    格式: [0xFF] [原始数据...] [0xFF]
            uint8_t echoBuf[BINARY_BUF_SIZE + 2];  // 最大 256 + 2 个 0xFF
            echoBuf[0] = 0xFF;
            memcpy(echoBuf + 1, rawData, len);
            echoBuf[len + 1] = 0xFF;
            parent->sendBinaryData(echoBuf, len + 2);
          }
        }
    };
};

#endif // BLUETOOTH_H