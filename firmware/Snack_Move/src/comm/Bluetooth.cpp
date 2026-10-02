#include "Bluetooth.h"

/**
 * @brief Bluetooth类构造函数
 * 初始化所有BLE相关指针为NULL，连接状态为false
 * 在对象创建时自动调用，确保所有成员变量处于已知状态
 */
Bluetooth::Bluetooth() 
{
    pServer = NULL;
    pCommandCharacteristic = NULL;
    pDataCharacteristic = NULL;
    deviceConnected = false;
    _binaryBufLen = 0;
    memset(_binaryBuf, 0, sizeof(_binaryBuf));
}

/**
 * @brief 蓝牙模块初始化函数
 * 完成BLE设备的初始化、服务创建、特征设置和广播启动
 * 这是蓝牙功能的核心初始化，必须在setup()中调用
 * 
 * 执行流程：
 * 1. 初始化BLE设备
 * 2. 创建服务器并设置回调
 * 3. 创建服务并添加特征
 * 4. 启动服务和广播
 */
void Bluetooth::begin() 
{
    
    // ========== BLE设备初始化 ==========
    // 初始化BLE设备栈，设置设备名称（客户端扫描时可见）
    BLEDevice::init(deviceName);
    
    // ========== BLE服务器创建 ==========
    // 创建BLE服务器实例，并设置连接状态回调
    // MyServerCallbacks(this)将当前对象指针传递给回调类，实现事件处理
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks(this));
    
    // ========== BLE服务创建 ==========
    // 创建主BLE服务，使用预定义的SERVICE_UUID
    // 一个服务可以包含多个特征，类似于文件夹包含多个文件
    BLEService* pService = pServer->createService(SERVICE_UUID);
    
    // ========== 命令特征创建 ==========
    // 创建命令特征，用于接收客户端发送的控制指令
    // PROPERTY_WRITE表示客户端可以向此特征写入数据
    pCommandCharacteristic = pService->createCharacteristic(
        COMMAND_CHARACTERISTIC_UUID,                    // 唯一标识符
        BLECharacteristic::PROPERTY_WRITE               // 可写属性
    );
    // 设置命令特征的写入回调函数，处理接收到的数据
    pCommandCharacteristic->setCallbacks(new CommandCallbacks(this));
    
    // ========== 数据特征创建 ==========
    // 创建数据特征，用于向客户端发送状态信息和通知
    // PROPERTY_READ | PROPERTY_NOTIFY 表示可读且支持通知
    pDataCharacteristic = pService->createCharacteristic(
        DATA_CHARACTERISTIC_UUID,                       // 唯一标识符
        BLECharacteristic::PROPERTY_READ |              // 可读属性
        BLECharacteristic::PROPERTY_NOTIFY              // 通知属性（主动推送数据）
    );
    // 添加BLE2902描述符，启用通知功能（客户端订阅后可以接收服务器推送）
    pDataCharacteristic->addDescriptor(new BLE2902());
    
    // ========== BLE服务启动 ==========
    // 启动BLE服务，服务必须在广播前启动
    // 启动后特征才可被客户端访问
    pService->start();
    
    // ========== 开始广播 ==========
    // 启动BLE广播，使设备可被客户端扫描和发现
    // 广播包含设备名称、服务UUID等信息
    pServer->getAdvertising()->start();
    
    // ========== 初始化完成提示 ==========
    Serial.println("BLE Server started, waiting for commands...");
    // Serial.println("Send 11 for Red, 22 for Green, 33 for Blue");
    // 注意：RGB初始化显示已移至主程序，确保模块职责单一
}


/**
 * @brief 向已连接的BLE客户端发送数据
 * 通过数据特征以通知方式向客户端推送数据
 * 支持实时状态反馈、调试信息、传感器数据等
 * 
 * @param data 要发送的字符串数据，自动转换为C风格字符串
 * 
 * 注意事项：
 * - 仅在设备已连接时发送数据
 * - 数据长度受BLE MTU限制（通常20-512字节）
 * - 通知需要客户端已启用订阅
 */
void Bluetooth::sendDataToClient(String data) 
{
    // 检查设备连接状态和数据特征有效性
    if (deviceConnected && pDataCharacteristic != NULL) 
    {
        // 设置特征值（要发送的数据）
        pDataCharacteristic->setValue(data.c_str());
        // 发送通知，主动推送数据到已订阅的客户端
        pDataCharacteristic->notify();
        // // 串口输出发送确认，便于调试
        // Serial.println("Sent to client: " + data);
    } 
    else 
    {
        // 连接断开或特征未初始化时的错误处理
        Serial.println("Cannot send data: Device not connected");
    }
}

/**
 * @brief 向已连接的BLE客户端发送二进制数据
 * @param data 原始字节数组
 * @param len  数据长度
 */
void Bluetooth::sendBinaryData(const uint8_t* data, uint16_t len) {
    if (deviceConnected && pDataCharacteristic != NULL && data != NULL && len > 0) {
        pDataCharacteristic->setValue((uint8_t*)data, len);
        pDataCharacteristic->notify();
        // Serial.printf("[BLE] 发送二进制 %d 字节\n", len);
    } else {
        Serial.println("[BLE] 无法发送二进制数据：设备未连接");
    }
}

/**
 * @brief 设置BLE设备广播名称
 * 修改设备在客户端扫描列表中显示的名称
 * 必须在begin()调用前设置才能生效
 * 
 * @param name 新的设备名称，以空字符结尾的C风格字符串
 * 
 * 命名建议：
 * - 长度适中，易于识别
 * - 避免特殊字符
 * - 体现设备功能，如"ESP32_Light_Controller"
 */
void Bluetooth::setDeviceName(const char* name) 
{
    deviceName = name;  // 更新设备名称指针
}

/**
 * @brief 获取当前BLE连接状态
 * 用于主程序判断是否可以进行数据通信
 * 
 * @return bool true=设备已连接，false=设备未连接
 * 
 * 使用场景：
 * - 条件性执行需要连接的功能
 * - 连接状态显示（如LED指示灯）
 * - 数据发送前的状态检查
 */
bool Bluetooth::isConnected() 
{
    return deviceConnected;  // 返回当前连接状态标志
}

void Bluetooth::onCommandReceived(String command) 
{
    if (_userCommandCallback != nullptr) 
    {
        // 如果用户设置了自定义回调，则调用用户的处理逻辑
        _userCommandCallback(command);
    } 
    else
    {
        Serial.println("No user callback set, received Data: ");
        Serial.print("[String]:");
        Serial.println(command);
        Serial.print("[Hex]: ");
        for (size_t i = 0; i < command.length(); i++) {
            Serial.printf("%02X ", (uint8_t)command[i]);
        }
        Serial.println();
        delay(1);
    }
}

/**
 * @brief 蓝牙主循环处理函数
 * 在loop()中周期性调用，用于处理需要持续运行的后台任务
 * 当前版本为空实现，预留用于未来功能扩展
 * 如：连接状态监测、定时数据发送、重连机制等
 */

void Bluetooth::loop() 
{
    // 如果有用户设置的主循环回调，则执行
    if (_userLoopCallback != nullptr) 
    {
        _userLoopCallback();
    }
    // 库本身可以在这里保留一些必要的后台处理（如连接状态监控）
}

// ============================ 回调函数设置方法实现 ============================

void Bluetooth::setCommandCallback(void (*callback)(String command)) {
    _userCommandCallback = callback;
}

void Bluetooth::setBinaryCallback(void (*callback)(uint8_t* data, uint16_t len)) {
    _userBinaryCallback = callback;
}

void Bluetooth::setOnConnectCallback(void (*callback)(void)) {
    _userOnConnectCallback = callback;
}

void Bluetooth::setOnDisconnectCallback(void (*callback)(void)) {
    _userOnDisconnectCallback = callback;
}

void Bluetooth::setLoopCallback(void (*callback)(void)) {
    _userLoopCallback = callback;
}