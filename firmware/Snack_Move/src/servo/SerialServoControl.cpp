#include "SerialServoControl.h"

// ======================================================================
//  SingleServoManager 实现
// ======================================================================

// 构造函数
SingleServoManager::SingleServoManager(HardwareSerial &serial, uint8_t servoId)
    : m_serial(serial)
    , m_servoId(servoId)
    , m_state(STATE_IDLE)
    , m_targetPosition(0)
    , m_moveTime(0)
    , m_tolerance(5)
{
};

// 立即移动舵机（不等待完成）
void SingleServoManager::moveImmediate(int16_t position, uint16_t time) {    
    // 发送立即执行指令
    LobotSerialServoMove(m_serial, m_servoId, position, time);
    
    // 更新状态
    m_state = STATE_MOVING;
    m_targetPosition = position;
    m_moveTime = time;
}

// 顺序移动舵机（等待上一个动作完成）
void SingleServoManager::moveSequential(int16_t position, uint16_t time, uint16_t timeout, int16_t tolerance) 
{
  //更新舵机当前状态
  updateStateAfterMove();
  // 如果舵机正在运动，等待完成
  if (m_state == STATE_MOVING) {
      // 计算超时时间
      uint16_t waitTimeout = (timeout > 0) ? timeout : calculateTimeout(m_moveTime);
      unsigned long waitStartTime = millis();
      
      // 等待当前动作完成
      while (millis() - waitStartTime < waitTimeout) {
          if (checkPositionReached(m_targetPosition, tolerance)) { 
              // 当前动作已完成
              m_state = STATE_IDLE;
              break;
          }
          delay(10);  // 短暂延时
      }
      // 如果超时，强制停止
      if (m_state == STATE_MOVING) {
          stop();
          m_state = STATE_IDLE;
      }
  }
  
  // 发送新动作指令
  LobotSerialServoMove(m_serial, m_servoId, position, time);
  
  // 更新状态
  m_state = STATE_MOVING;
  m_targetPosition = position;
  m_moveTime = time;
}

// 预设舵机位置（等待START指令）
void SingleServoManager::presetPosition(int16_t position, uint16_t time) {
    // 发送预设指令（指令7）
    LobotSerialServoMoveTimeWait(m_serial, m_servoId, position, time);
    
    // 短暂延时，确保舵机有时间处理预设指令，避免紧接着的START指令失败
    delay(10);
    
    // 记录预设参数（用于顺序运动的超时判断）
    m_targetPosition = position;
    m_moveTime = time;
}

// 启动预设的舵机运动
bool SingleServoManager::startPreset() {
    // 发送启动指令（指令11）
    LobotSerialServoMoveStart(m_serial, m_servoId);
    
    // 更新状态
    m_state = STATE_MOVING;
    
    return true;
}

// 停止当前运动
void SingleServoManager::stop() {
    LobotSerialServoStopMove(m_serial, m_servoId);
    m_state = STATE_IDLE;
}

// 读取舵机当前位置
int16_t SingleServoManager::getCurrentPosition() {
    int16_t pos = LobotSerialServoReadPosition(m_serial, m_servoId);
    return pos;
}

// 读取舵机状态
SingleServoManager::ServoState SingleServoManager::getState() const {
    return m_state;
}

// 检查舵机是否空闲
bool SingleServoManager::isIdle() const {
    return m_state == STATE_IDLE;
}

// 检查舵机是否在运动
bool SingleServoManager::isMoving() const {
    return (m_state == STATE_MOVING);
}

// 设置位置容差
void SingleServoManager::setTolerance(int16_t tolerance) {
    m_tolerance = (tolerance < 0) ? 0 : tolerance;
}

// 获取目标位置
int16_t SingleServoManager::getTargetPosition() const {
    return m_targetPosition;
}


// 私有辅助函数：检查是否到达目标位置
bool SingleServoManager::checkPositionReached(int16_t targetPos, int16_t tolerance) {
    int16_t currentPos = getCurrentPosition();
    
    if (currentPos == -2048) {
        return false;  // 读取失败
    }
    // Serial.print("currentPos: ");Serial.println(currentPos);
    // Serial.print("targetPos: ");Serial.println(targetPos);
    int16_t error = abs(currentPos - targetPos);
    // Serial.print("error: ");Serial.println(error);
    // Serial.print("tolerance: ");Serial.println(tolerance);
    return (error <= tolerance);
}

// 私有辅助函数：更新移动后的状态
void SingleServoManager::updateStateAfterMove() {
    if (m_state == STATE_MOVING) {
        if (checkPositionReached(m_targetPosition, m_tolerance)) {
            m_state = STATE_IDLE;
        }
    }
}

// 私有辅助函数：计算超时时间
uint16_t SingleServoManager::calculateTimeout(uint16_t moveTime) const {
    return moveTime + 100;  // 运动时间 + 0.1秒余量
}

// ======================================================================
//  ServoBus 全局舵机总线池 — 静态成员 + 实现
// ======================================================================

uint8_t  ServoBus::s_buffer[SERVO_POOL_SIZE * sizeof(SingleServoManager)];
bool     ServoBus::s_initialized = false;
HardwareSerial* ServoBus::s_serial = nullptr;
uint8_t  ServoBus::s_busIds[SERVO_POOL_SIZE] = {};
uint8_t  ServoBus::s_busCount = 0;

void ServoBus::init(HardwareSerial &serial) {
    if (s_initialized) return;
    s_serial = &serial;
    for (uint8_t i = 0; i < SERVO_POOL_SIZE; i++) {
        new (s_buffer + i * sizeof(SingleServoManager))
            SingleServoManager(serial, i);
    }
    s_initialized = true;
    s_busCount = 0;
}

SingleServoManager* ServoBus::get(uint8_t id) {
    if (id >= SERVO_POOL_SIZE || !s_initialized) return nullptr;
    return reinterpret_cast<SingleServoManager*>(
        s_buffer + id * sizeof(SingleServoManager));
}

void ServoBus::setBusInfo(const uint8_t* ids, uint8_t count) {
    s_busCount = (count > SERVO_POOL_SIZE) ? SERVO_POOL_SIZE : count;
    for (uint8_t i = 0; i < s_busCount; i++) {
        s_busIds[i] = ids[i];
    }
}

// 获取总线舵机数量（广播模式需要）
uint8_t ServoBus::busCount() {
    return s_busCount;
}

// 获取总线上所有舵机的真实 ID 列表（广播模式需要）
uint8_t ServoBus::getBusIds(uint8_t* idsOut) {
    for (uint8_t i = 0; i < s_busCount; i++) {
        idsOut[i] = s_busIds[i];
    }
    return s_busCount;
}

// 广播启动所有预设（指令11, ID=254）
void ServoBus::broadcastStart() {
    if (s_serial == nullptr) return;
    LobotSerialServoMoveStart(*s_serial, LOBOT_SERVO_ID_ALL);
}

// ======================================================================
//  ServoGroupManager 实现（改造版 — 存储ID，通过 ServoBus 获取对象）
//  广播模式实现 (SERVO_GROUP_MODE_BROADCAST)
// ======================================================================
#ifdef SERVO_GROUP_MODE_BROADCAST

ServoGroupManager::ServoGroupManager()
    : m_memberCount(0)
{
}

void ServoGroupManager::addServo(uint8_t servoId) {
    if (m_memberCount >= MAX_SERVO_COUNT) return;
    if (findMember(servoId) >= 0) return;
    m_memberIds[m_memberCount] = servoId;
    m_memberCount++;
}

void ServoGroupManager::presetAll(const GroupServoAction actions[], uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        SingleServoManager* servo = ServoBus::get(actions[i].servoId);
        if (servo != nullptr) {
            servo->presetPosition(actions[i].position, actions[i].moveTime);
        }
    }
}

bool ServoGroupManager::isAllIdle() const {
    for (uint8_t i = 0; i < m_memberCount; i++) {
        SingleServoManager* servo = ServoBus::get(m_memberIds[i]);
        if (servo != nullptr && !servo->isIdle()) {
            return false;
        }
    }
    return true;
}

void ServoGroupManager::stopAll() {
    for (uint8_t i = 0; i < m_memberCount; i++) {
        SingleServoManager* servo = ServoBus::get(m_memberIds[i]);
        if (servo != nullptr) {
            servo->stop();
        }
    }
}

SingleServoManager* ServoGroupManager::getServo(uint8_t id) const {
    return ServoBus::get(id);
}

// 查找成员索引，-1=不存在
int8_t ServoGroupManager::findMember(uint8_t id) const {
    for (uint8_t i = 0; i < m_memberCount; i++) {
        if (m_memberIds[i] == id) return i;
    }
    return -1; 
}

bool ServoGroupManager::isInGroup(uint8_t id) const {
    return findMember(id) >= 0;
}

void ServoGroupManager::startAllBroadcast() {
    // Step 1: 从 ServoBus 获取总线上所有舵机的真实 ID，中和非组内舵机
    uint8_t allIds[SERVO_POOL_SIZE];
    uint8_t allCount = ServoBus::getBusIds(allIds);   

    for (uint8_t i = 0; i < allCount; i++) {
        uint8_t id = allIds[i];
        if (!isInGroup(id)) {
            SingleServoManager* servo = ServoBus::get(id);
            if (servo != nullptr) {
                int16_t currentPos = servo->getCurrentPosition();
                if (currentPos >= 0) {
                    servo->presetPosition(currentPos, 0);
                }
            }
        }
    }

    // Step 2: 广播启动所有预设
    ServoBus::broadcastStart();

    // Step 3: 更新组内舵机状态为 MOVING
    for (uint8_t i = 0; i < m_memberCount; i++) {
        SingleServoManager* servo = ServoBus::get(m_memberIds[i]);
        if (servo != nullptr) {
            servo->setStateMoving();
        }
    }
}

void ServoGroupManager::moveAllSequentialBroadcast(const GroupServoAction actions[], uint8_t count) {
    // Step 1: 等待组内每个舵机完成当前运动
    for (uint8_t i = 0; i < m_memberCount; i++) {
        SingleServoManager* servo = ServoBus::get(m_memberIds[i]);
        if (servo == nullptr) continue;

        servo->updateState();

        if (servo->isMoving()) {
            uint16_t waitTimeout = servo->getMoveTime() + 1000;
            unsigned long waitStartTime = millis();

            while (millis() - waitStartTime < waitTimeout) {
                if (servo->checkTargetReached(5)) {
                    servo->setStateIdle();
                    break;
                }
                delay(10);
            }

            if (servo->isMoving()) {
                servo->stop();
            }
        }
        servo->setStateIdle();
    }

    // Step 2: 预设所有舵机
    presetAll(actions, count);

    // Step 3: 广播启动
    startAllBroadcast();
}

void ServoGroupManager::moveAllImmediateBroadcast(const GroupServoAction actions[], uint8_t count) {
    presetAll(actions, count);
    startAllBroadcast();
}

#endif // SERVO_GROUP_MODE_BROADCAST

// ======================================================================
//  逐个模式实现 (SERVO_GROUP_MODE_INDIVIDUAL)
// ======================================================================
#ifdef SERVO_GROUP_MODE_INDIVIDUAL

void moveAllSequentialIndividual(const GroupServoAction actions[], uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        SingleServoManager* servo = ServoBus::get(actions[i].servoId);
        if (servo != nullptr) {
            servo->moveSequential(actions[i].position, actions[i].moveTime);
        }
    }
}

void moveAllImmediateIndividual(const GroupServoAction actions[], uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        SingleServoManager* servo = ServoBus::get(actions[i].servoId);
        if (servo != nullptr) {
            servo->moveImmediate(actions[i].position, actions[i].moveTime);
        }
    }
}

#endif // SERVO_GROUP_MODE_INDIVIDUAL