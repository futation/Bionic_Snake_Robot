#ifndef SERIALSERVOCONTROL_H
#define SERIALSERVOCONTROL_H

#include <Arduino.h>
#include "SerialServoMove.h"
#include "SerialServoInclude.h"
#include <new>

// ======================== 常量 ========================
#define MAX_SERVO_COUNT      12   // 舵机组最大成员数
#define SERVO_POOL_SIZE      21   // 舵机池大小（ID 0~20），用于广播模式的总线管理

/**
 * 单舵机管理器类
 * 提供多种运动控制方式，内部维护舵机状态
 */
class SingleServoManager {
public:
    // 舵机状态枚举
    enum ServoState {
        STATE_IDLE,      // 空闲状态
        STATE_MOVING,    // 运动状态
    };    
    
    // 构造函数
    SingleServoManager(HardwareSerial &serial, uint8_t servoId);
    
    // 立即移动舵机（不等待完成）
    // 参数：position-目标位置(0-1000), time-运动时间(0-30000ms)
    // 返回值：是否成功发送指令
    void moveImmediate(int16_t position, uint16_t time);
    
    // 顺序移动舵机（等待上一个动作完成）
    // 参数：position-目标位置(0-1000), time-运动时间(0-30000ms)
    //       timeout-超时时间(0=自动计算), tolerance-位置容差
    // 返回值：移动结果(1=成功, -1=超时, -2=读取位置失败, -3=参数错误)
    void moveSequential(int16_t position, uint16_t time, uint16_t timeout = 0, int16_t tolerance = 10);
    
    // 预设舵机位置（等待START指令）
    // 参数：position-目标位置(0-1000), time-运动时间(0-30000ms)
    void presetPosition(int16_t position, uint16_t time);
    
    // 启动预设的舵机运动
    bool startPreset();
    
    // 停止当前运动
    void stop();
    
    // 读取舵机当前位置
    int16_t getCurrentPosition();
    
    // 读取舵机状态
    ServoState getState() const;
    
    // 检查舵机是否空闲
    bool isIdle() const;
    
    // 检查舵机是否在运动
    bool isMoving() const;
    
    // 设置位置容差
    void setTolerance(int16_t tolerance);    
    
    // 获取目标位置
    int16_t getTargetPosition() const;

    // 获取运动时间
    uint16_t getMoveTime() const { return m_moveTime; }

    // 获取舵机ID
    uint8_t getServoId() const { return m_servoId; }

    // 检查是否到达目标位置（公开包装）
    bool checkTargetReached(int16_t tolerance) { return checkPositionReached(m_targetPosition, tolerance); }
    
    // 手动设置运动状态（供 ServoGroupManager 广播启动时使用）
    void setStateMoving() { m_state = STATE_MOVING; }

    // 手动设置空闲状态
    void setStateIdle() { m_state = STATE_IDLE; }

    // 刷新运动状态（读取当前位置，若到达目标则切换为 IDLE）
    void updateState() { updateStateAfterMove(); }
    
    
private:
    HardwareSerial &m_serial;     // 串口引用
    uint8_t m_servoId;           // 舵机ID
    
    ServoState m_state;          // 当前状态
    int16_t m_targetPosition;    // 目标位置（用于顺序运动判断）
    uint16_t m_moveTime;         // 运动时间（用于顺序运动判断）
    
    int16_t m_tolerance;         // 位置容差
    
    // 私有辅助函数
    bool checkPositionReached(int16_t targetPos, int16_t tolerance);
    void updateStateAfterMove();
    uint16_t calculateTimeout(uint16_t moveTime) const;
};

// ======================== 条件编译开关 ========================
// 用户需在 #include "SerialServoControl.h" 之前定义以下宏之一：
// #define SERVO_GROUP_MODE_BROADCAST    // 精细：广播启动模式（需 setBusIds）
#define SERVO_GROUP_MODE_INDIVIDUAL   // 简单：逐个启动模式

// 可同时定义两个，所有方法均可用。都不定义时默认启用 INDIVIDUAL 模式。
#if !defined(SERVO_GROUP_MODE_BROADCAST) && !defined(SERVO_GROUP_MODE_INDIVIDUAL)
#define SERVO_GROUP_MODE_INDIVIDUAL
#endif


// ======================== 舵机动作定义 ========================

/**
 * @struct GroupServoAction
 * @brief  单条舵机动作指令（ID + 位置 + 时间），用于批量传入舵机组
 */
struct GroupServoAction {
    uint8_t  servoId;      // 舵机ID
    int16_t  position;     // 目标位置 (0~1000)
    uint16_t moveTime;     // 运动时间 (0~30000 ms)
};

// ======================== 全局舵机总线池 ========================

/**
 * @class ServoBus
 * @brief  全局舵机对象池 —— 单例，所有 SingleServoManager 的集中构造点
 *
 * 项目启动时调用 ServoBus::init(serial) 在预分配静态内存上
 * 用 placement new 构造所有 SingleServoManager。
 * 任何模块需要操控舵机时，通过 ServoBus::get(id) 获取唯一指针。
 * 这样 ServoGroupManager 和扫描/校准模块共享同一套舵机状态。
 */
class ServoBus {
public:
    /** @brief 初始化池 — setup() 中调用一次 */
    static void init(HardwareSerial &serial);

    /** @brief 获取 id 号舵机的管理器（nullptr = 无效ID） */
    static SingleServoManager* get(uint8_t id);

    /** @brief 设置总线信息（由外部传入信息）
     *  @param ids   真实 ID 数组
     *  @param count 舵机数量 */
    static void setBusInfo(const uint8_t* ids, uint8_t count);

    /** @brief 获取总线舵机数量 */
    static uint8_t busCount();

    /** @brief 获取总线上所有舵机的真实 ID 列表 */
    static uint8_t getBusIds(uint8_t* idsOut);

    /** @brief 广播启动所有预设（指令11, ID=254） */
    static void broadcastStart();

private:
    static uint8_t         s_buffer[SERVO_POOL_SIZE * sizeof(SingleServoManager)];
    static bool            s_initialized;
    static HardwareSerial* s_serial;                     // init 时保存的串口
    static uint8_t         s_busIds[SERVO_POOL_SIZE];   // 保存真实 ID
    static uint8_t         s_busCount;                  // 实际数量
};


// ======================== 舵机组管理类（改造版） ========================

/**
 * @class ServoGroupManager
 * @brief  管理一组舵机，支持同步运动（presetAll + startAll）和顺序运动
 *
 * 改造要点：不再自行 new SingleServoManager，改为存储舵机 ID，
 * 所有舵机操作通过 ServoBus::get(id) 获取唯一的对象实例。
 *
 * 两种控制模式（条件编译）：
 *   - 广播模式 (SERVO_GROUP_MODE_BROADCAST)：广播 START(ID=254)，
 *     自动从 ServoBus 获取总线全貌，中和非组内舵机。
 *   - 逐个模式 (SERVO_GROUP_MODE_INDIVIDUAL)：逐个发送 START 指令。
 *
 * 用法示例：
 * @code
 *   #define SERVO_GROUP_MODE_BROADCAST
 *   #include "SerialServoControl.h"
 *
 *   ServoBus::init(Serial1);
 *   uint8_t busIds[] = {0,1,2,3,4,5};  // 扫描得到的真实 ID
 *   ServoBus::setBusInfo(busIds, 6);    // 外部传入总线信息
 *
 *   ServoGroupManager group;
 *   group.addServo(0);
 *   group.addServo(2);
 *
 *   GroupServoAction actions[] = {{0,500,1000}, {2,800,1000}};
 *   group.moveAllSequentialBroadcast(actions, 2);
 * @endcode
 */

#ifdef SERVO_GROUP_MODE_BROADCAST
class ServoGroupManager {
public:
    ServoGroupManager();

    // ======================== 通用接口（两模式共享） ========================

    /** @brief 添加舵机到组（只记 ID，不创建对象） */
    void addServo(uint8_t servoId);

    /** @brief 预设组内所有舵机的位置 */
    void presetAll(const GroupServoAction actions[], uint8_t count);

    /** @brief 检查组内所有舵机是否空闲 */
    bool isAllIdle() const;

    /** @brief 停止组内所有舵机 */
    void stopAll();

    // ======================== 广播模式接口 ========================

    /** @brief 广播启动所有预设
     *  @note  内部流程：
     *         1. 通过 ServoBus::getBusIds() 获取总线全貌 → 中和非组内舵机
     *         2. 广播 START(ID=254)
     *         3. 更新组内舵机状态 */
    void startAllBroadcast();

    void moveAllSequentialBroadcast(const GroupServoAction actions[], uint8_t count);

    /** @brief 立即执行组动作 → 预设 → 广播启动
     *  @note  中和非组内舵机通过 startAllBroadcast() 完成 */
    void moveAllImmediateBroadcast(const GroupServoAction actions[], uint8_t count);

private:
    uint8_t  m_memberIds[MAX_SERVO_COUNT];   // 只存 ID，不自建对象
    uint8_t  m_memberCount;

    /** @brief 通过 ServoBus 获取舵机管理器指针 */
    SingleServoManager* getServo(uint8_t id) const;

    /** @brief 在 m_memberIds 中查找 id，返回下标，-1=不存在 */
    int8_t findMember(uint8_t id) const;

    /** @brief 判断指定ID是否在组内 */
    bool isInGroup(uint8_t id) const;
};
#endif // SERVO_GROUP_MODE_BROADCAST

    // ======================== 逐个模式接口 ========================
#ifdef SERVO_GROUP_MODE_INDIVIDUAL
    /** @brief 顺序执行组动作 → 依次对每个舵机调用 moveSequential（阻塞） */
    void moveAllSequentialIndividual(const GroupServoAction actions[], uint8_t count);

    /** @brief 立即执行组动作 → 依次对每个舵机调用 moveImmediate（不等待） */
    void moveAllImmediateIndividual(const GroupServoAction actions[], uint8_t count);

#endif // SERVO_GROUP_MODE_INDIVIDUAL


#endif