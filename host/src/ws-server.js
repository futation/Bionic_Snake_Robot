/**
 * WebSocket 服务器与消息路由模块
 * 监听本地端口，处理前端 JSON 请求，控制 BLE 和协议操作
 */

const WebSocket = require('ws');
const {
  CMD,
  buildAdjustServo,
  buildSaveServo,
  buildChangeId,
  buildActionStep,
  buildSimpleCmd,
  parseFeedback,
  parseServoParams,
  parseScanResult,
  toHexString,
  fromHexString,
} = require('./protocol');

class WSServer {
  constructor(bleManager, storage) {
    this.ble = bleManager;
    this.storage = storage;
    this.wss = null;
    this.port = 0;
    this.clients = new Set();

    // 动作组播放状态
    this.playbackState = {
      isPlaying: false,
      isPaused: false,
      steps: [],
      currentStepIndex: 0,
      loopState: null,
      timerId: null,
      uiLoop: false,
    };

    this._setupBLEListeners();
  }

  _setupBLEListeners() {
    // BLE 事件 → WebSocket 事件转发
    this.ble.on('deviceDiscovered', (dev) => {
      this.broadcast('ble:deviceList', {
        devices: [{
          name: dev.name,
          mac: dev.mac,
          rssi: dev.rssi,
          delay: dev.delay || 0,
        }],
      });
    });

    this.ble.on('connected', (device) => {
      this.broadcast('ble:connected', {
        name: device.name,
        mac: device.mac,
      });

      // 发送服务列表
      setTimeout(() => {
        this.broadcast('ble:services', {
          services: this._formatServices(),
        });
      }, 500);
    });

    this.ble.on('disconnected', () => {
      this.broadcast('ble:disconnected', {});
    });

    this.ble.on('dataReceived', (data) => {
      this.broadcast('ble:dataReceived', data);
    });

    this.ble.on('globalBindUpdated', (bindInfo) => {
      this.broadcast('globalBind:updated', bindInfo);
    });

    this.ble.on('log', (log) => {
      this.broadcast('log:message', log);
    });

    this.ble.on('status', (status) => {
      this.broadcast('status:update', status);
    });
  }

  _formatServices() {
    const result = {};
    for (const [sUuid, service] of Object.entries(this.ble.services)) {
      result[sUuid] = {
        name: service.name,
        characteristics: service.characteristics.map(c => ({
          uuid: c.uuid,
          name: c.name || c.uuid.substring(0, 8) + '...',
          properties: c.properties.join(', '),
        })),
      };
    }
    return result;
  }

  /**
   * 启动 WebSocket 服务器（独立端口）
   * @returns {Promise<number>} 分配的端口号
   */
  async start(port = 0) {
    return new Promise((resolve, reject) => {
      this.wss = new WebSocket.Server({
        host: '127.0.0.1',
        port: port || 0,
      });

      this.wss.on('listening', () => {
        this.port = this.wss.address().port;
        console.log(`[WS] WebSocket 服务器已启动: ws://127.0.0.1:${this.port}`);
        resolve(this.port);
      });

      this.wss.on('error', (err) => {
        console.error('[WS] 服务器错误:', err);
        reject(err);
      });

      this._setupConnectionHandler();
    });
  }

  /**
   * 挂载到现有 HTTP 服务器（共用端口）
   * @param {http.Server} httpServer
   */
  async startOnServer(httpServer) {
    this.wss = new WebSocket.Server({ server: httpServer });
    this.port = httpServer.address().port;
    console.log(`[WS] WebSocket 已挂载到 HTTP 端口: ${this.port}`);
    this._setupConnectionHandler();
    return this.port;
  }

  _setupConnectionHandler() {
    this.wss.on('connection', (ws) => {
        this.clients.add(ws);
        console.log(`[WS] 客户端已连接 (当前 ${this.clients.size} 个)`);

        // 发送当前状态
        ws.send(JSON.stringify({
          type: 'status:update',
          data: {
            scanInfo: this.ble.isScanning ? '正在扫描...' : '已停止',
            adapterInfo: this.ble.connectedDevice
              ? `已连接 [${this.ble.connectedDevice.name}]`
              : '蓝牙适配器: 未连接',
          },
        }));

        // 发送全局绑定状态
        ws.send(JSON.stringify({
          type: 'globalBind:updated',
          data: {
            txChar: this.ble.globalTxChar,
            rxChar: this.ble.globalRxChar,
          },
        }));

        ws.on('message', (raw) => {
          try {
            const msg = JSON.parse(raw.toString());
            this._handleMessage(ws, msg);
          } catch (e) {
            console.error('[WS] 消息解析失败:', e.message);
            ws.send(JSON.stringify({
              type: 'log:message',
              data: { level: 'error', message: `消息解析失败: ${e.message}` },
            }));
          }
        });

        ws.on('close', () => {
          this.clients.delete(ws);
          console.log(`[WS] 客户端已断开 (当前 ${this.clients.size} 个)`);
        });

        ws.on('error', (err) => {
          console.error('[WS] 客户端错误:', err.message);
        });
      });
  }

  /**
   * 广播消息到所有客户端
   */
  broadcast(type, data) {
    const message = JSON.stringify({ type, data });
    for (const ws of this.clients) {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(message);
      }
    }
  }

  /**
   * 发送消息到特定客户端
   */
  _send(ws, type, data) {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type, data }));
    }
  }

  // ==================== 消息处理 ====================
  async _handleMessage(ws, msg) {
    const { type, data = {} } = msg;
    // 不再记录 [收到指令]，因为 framework.html 已在 handleIframeRequest 中记录 [请求]

    try {
      switch (type) {
        // === BLE 操作 ===
        case 'ble:scan':
          await this.ble.startScan();
          break;

        case 'ble:stopScan':
          await this.ble.stopScan();
          break;

        case 'ble:connect':
          await this._handleBleConnect(ws, data);
          break;

        case 'ble:disconnect':
          await this.ble.disconnect();
          break;

        case 'ble:write':
          await this.ble.writeCharacteristic(
            data.serviceUuid, data.charUuid, data.data, data.hex !== false
          );
          break;

        case 'ble:read':
          await this.ble.readCharacteristic(data.serviceUuid, data.charUuid);
          break;

        case 'ble:subscribe':
          await this.ble.subscribeCharacteristic(data.serviceUuid, data.charUuid);
          break;

        case 'ble:unsubscribe':
          await this.ble.unsubscribeCharacteristic(data.serviceUuid, data.charUuid);
          break;

        // === 全局绑定 ===
        case 'ble:bindTx':
          this.ble.bindTxChar(data.serviceUuid, data.charUuid);
          break;

        case 'ble:bindRx':
          this.ble.bindRxChar(data.serviceUuid, data.charUuid);
          break;

        case 'ble:unbindTx':
          this.ble.unbindTxChar();
          break;

        case 'ble:unbindRx':
          this.ble.unbindRxChar();
          break;

        // === 串口（暂为模拟） ===
        case 'serial:list':
          this._send(ws, 'serial:list', { ports: ['COM3', 'COM4', 'COM5'] });
          break;

        case 'serial:open':
          this._log('info', `[串口] 模拟打开: ${data.port} @ ${data.baudRate}`);
          break;

        case 'serial:write':
          await this._handleRawWrite(data);
          break;

        // === 原始发送 ===
        case 'command:sendRaw':
          await this._handleRawWrite(data);
          break;

        // === 校准操作 ===
        case 'calibration:enterMode':
          await this._handleEnterCalibration(ws);
          break;

        case 'calibration:exitMode':
          await this._handleExitCalibration(ws);
          break;

        case 'calibration:adjust':
          await this._handleCalibrationAdjust(data);
          break;

        case 'calibration:save':
          await this._handleCalibrationSave(ws, data);
          break;

        case 'calibration:getParams':
          await this._handleCalibrationGetParams(ws);
          break;

        case 'calibration:changeId':
          await this._handleCalibrationChangeId(ws, data);
          break;

        // === 动作组操作 ===
        case 'action:enterMode':
          await this._handleActionEnterMode(ws);
          break;

        case 'action:exitMode':
          await this._handleActionExitMode(ws);
          break;

        case 'action:powerOff':
          await this._handleActionPowerOff(ws);
          break;

        case 'action:scanPositions':
          await this._handleActionScanPositions(ws);
          break;

        case 'action:sendStep':
          await this._handleActionSendStep(data);
          break;

        case 'action:play':
          this._handleActionPlay(data);
          break;

        case 'action:pause':
          this._handleActionPause();
          break;

        case 'action:stop':
          this._handleActionStop();
          break;

        case 'action:singleStep':
          await this._handleActionSingleStep(data);
          break;

        // === 自定义控件 ===
        case 'control:sendPress':
          await this._handleControlSend(data, '按下');
          break;

        case 'control:sendRelease':
          await this._handleControlSend(data, '松开');
          break;

        case 'control:sendSwitch':
          await this._handleControlSend(data, `切换(${data.state})`);
          break;

        // === 数据持久化 ===
        case 'storage:save':
          await this.storage.save(data.key, data.value);
          this._send(ws, 'storage:saved', { key: data.key, success: true });
          break;

        case 'storage:load':
          const value = await this.storage.load(data.key);
          this._send(ws, 'storage:loaded', { key: data.key, value });
          break;

        // === 日志 ===
        case 'log:message':
          this._log(data.level || 'info', data.message);
          break;

        default:
          this._log('warn', `未知消息类型: ${type}`);
      }
    } catch (err) {
      this._log('error', `处理 ${type} 失败: ${err.message}`);
      this._send(ws, 'log:message', { level: 'error', message: err.message });
    }
  }

  // ==================== BLE 操作处理 ====================
  async _handleBleConnect(ws, data) {
    if (!data.mac) {
      throw new Error('缺少 MAC 地址');
    }
    await this.ble.connect(data.mac);
  }

  async _handleRawWrite(data) {
    if (!this.ble.globalTxChar && !this.ble.connectedDevice) {
      throw new Error('未连接设备或未绑定发送特征');
    }

    if (this.ble.globalTxChar) {
      await this.ble.writeViaGlobalTx(data.data || data.hex, data.hex !== false);
    } else {
      // 尝试直接发送 HEX 数据
      this._log('warn', '未绑定全局发送特征，数据无法发送');
      throw new Error('未绑定发送特征');
    }
  }

  // ==================== 校准操作 ====================
  async _handleEnterCalibration(ws) {
    this._log('info', '发送: 进入手动校准模式 (0x05)');
    const packet = buildSimpleCmd(CMD.ENTER_CALIBRATION);
    await this._sendPacket(packet, '进入校准模式');

    // 模拟下位机反馈
    setTimeout(() => {
      this._simulateCalibrationSuccess(ws);
    }, 500);
  }

  _simulateCalibrationSuccess(ws) {
    // 模拟 0x04 反馈
    this._log('info', '收到: 校准模式已激活 (0x04)');

    // 模拟 0x06 舵机参数上报
    const mockServos = [
      { id: 1, position: 500, offset: 0 },
      { id: 2, position: 520, offset: -20 },
      { id: 3, position: 480, offset: 12 },
    ];
    this.broadcast('calibration:servoList', { servos: mockServos });
    this._log('info', `收到: 舵机参数列表 (共 ${mockServos.length} 个)`);
  }

  async _handleExitCalibration(ws) {
    this._log('info', '发送: 退出手动校准模式 (0x09)');
    const packet = buildSimpleCmd(CMD.EXIT_CALIBRATION);

    setTimeout(() => {
      this._log('info', '收到: 已退出校准模式 (0x04)');
    }, 300);
  }

  async _handleCalibrationAdjust(data) {
    const { servoId, position, offset } = data;
    this._log('info', `发送: 实时调整 ID${servoId} Pos=${position} Offset=${offset}`);
    const packet = buildAdjustServo(servoId, position, offset || 0);
    await this._sendPacket(packet, `调整舵机${servoId}`);
  }

  async _handleCalibrationSave(ws, data) {
    const { servoId, position, offset } = data;
    this._log('info', `发送: 保存校准 ID${servoId} Pos=${position} Offset=${offset}`);
    const packet = buildSaveServo(servoId, position, offset || 0);

    setTimeout(() => {
      this.broadcast('calibration:saveResult', {
        success: true,
        servoId,
        message: `舵机${servoId} 保存成功`,
      });
    }, 400);
  }

  async _handleCalibrationGetParams(ws) {
    this._log('info', '发送: 读取参数 (0x08)');
    const packet = buildSimpleCmd(CMD.READ_PARAMS);

    setTimeout(() => {
      const mockServos = [
        { id: 1, position: 500, offset: 0 },
        { id: 2, position: 520, offset: -20 },
        { id: 3, position: 480, offset: 12 },
      ];
      this.broadcast('calibration:servoList', { servos: mockServos });
      this._log('info', `收到: 参数列表 (共 ${mockServos.length} 个)`);
    }, 400);
  }

  async _handleCalibrationChangeId(ws, data) {
    const { oldId, newId } = data;
    this._log('info', `发送: 修改ID ${oldId} → ${newId} (0x07)`);
    const packet = buildChangeId(oldId, newId);

    setTimeout(() => {
      this._log('info', `ID 修改成功: ${oldId} → ${newId}`);
      // 重新获取参数列表
      this._handleCalibrationGetParams(ws);
    }, 500);
  }

  // ==================== 动作组操作 ====================
  async _handleActionEnterMode(ws) {
    this._log('info', '发送: 进入动作组模式 (0x0A)');
    const packet = buildSimpleCmd(CMD.ENTER_ACTION);

    setTimeout(() => {
      this._log('info', '收到: 动作组模式已激活 (0x04)');
    }, 500);
  }

  async _handleActionExitMode(ws) {
    this._log('info', '发送: 退出动作组模式 (0x0B)');
    const packet = buildSimpleCmd(CMD.EXIT_ACTION);

    // 停止播放
    this._handleActionStop();

    setTimeout(() => {
      this._log('info', '收到: 已退出动作组模式 (0x04)');
    }, 300);
  }

  async _handleActionPowerOff(ws) {
    this._log('info', '发送: 舵机掉电 (0x0E)');
    const packet = buildSimpleCmd(CMD.POWER_OFF);

    setTimeout(() => {
      this._log('info', '收到: 舵机已掉电 (0x04)');
    }, 400);
  }

  async _handleActionScanPositions(ws) {
    this._log('info', '发送: 扫描位置 (0x0C)');
    const packet = buildSimpleCmd(CMD.SCAN_POSITIONS);

    // 模拟扫描结果
    setTimeout(() => {
      const mockPositions = [
        { id: 1, position: 500 },
        { id: 2, position: 620 },
        { id: 3, position: 480 },
        { id: 4, position: 510 },
        { id: 5, position: 550 },
        { id: 6, position: 490 },
      ];
      this.broadcast('action:scannedPositions', { positions: mockPositions });
      this._log('info', `收到: 扫描结果 (共 ${mockPositions.length} 个舵机)`);
    }, 800);
  }

  async _handleActionSendStep(data) {
    if (!data.step) throw new Error('缺少 step 数据');
    const servos = data.step.servos || [];
    this._log('info', `发送: 动作组 (${servos.length} 个舵机)`);
    const packet = buildActionStep(servos);
    await this._sendPacket(packet, '动作组');
  }

  // ==================== 动作组播放 ====================
  _handleActionPlay(data) {
    const { steps, loop } = data;

    if (!steps || !Array.isArray(steps) || steps.length === 0) {
      throw new Error('无效的播放数据');
    }

    // 停止之前的播放
    this._handleActionStop();

    this.playbackState.isPlaying = true;
    this.playbackState.isPaused = false;
    this.playbackState.steps = steps;
    this.playbackState.currentStepIndex = 0;
    this.playbackState.loopState = null;
    this.playbackState.uiLoop = loop || false;
    this.playbackState.jsonLoop = data.jsonLoop || null;

    this._log('info', `开始播放: ${steps.length} 个步骤`);
    this._scheduleNextStep();
  }

  _handleActionPause() {
    if (!this.playbackState.isPlaying) return;
    this.playbackState.isPlaying = false;
    this.playbackState.isPaused = true;

    if (this.playbackState.timerId) {
      clearTimeout(this.playbackState.timerId);
      this.playbackState.timerId = null;
    }

    this._log('info', `播放已暂停 - Step ${this.playbackState.currentStepIndex + 1}`);
    this.broadcast('action:playbackProgress', {
      currentStep: this.playbackState.currentStepIndex,
      totalSteps: this.playbackState.steps.length,
      status: 'paused',
    });
  }

  _handleActionStop() {
    this.playbackState.isPlaying = false;
    this.playbackState.isPaused = false;
    this.playbackState.currentStepIndex = 0;
    this.playbackState.loopState = null;

    if (this.playbackState.timerId) {
      clearTimeout(this.playbackState.timerId);
      this.playbackState.timerId = null;
    }

    this._log('info', '播放已停止');
    this.broadcast('action:playbackProgress', {
      currentStep: 0,
      totalSteps: this.playbackState.steps.length,
      status: 'stopped',
    });
  }

  async _handleActionSingleStep(data) {
    const { index } = data;
    const steps = this.playbackState.steps;
    if (!steps || index >= steps.length) return;

    await this._executeStep(index);

    this.broadcast('action:playbackProgress', {
      currentStep: index,
      totalSteps: steps.length,
      status: 'singleStep',
    });
  }

  async _executeStep(idx) {
    const step = this.playbackState.steps[idx];
    if (!step) return;

    this._log('info', `执行 Step ${idx + 1}/${this.playbackState.steps.length}`);
    this.broadcast('action:playbackProgress', {
      currentStep: idx,
      totalSteps: this.playbackState.steps.length,
      status: 'playing',
    });

    // 构造并发送动作组指令
    try {
      const packet = buildActionStep(step.servos || []);
      await this._sendPacket(packet, `动作组 Step${idx + 1}`);
    } catch (e) {
      this._log('error', `发送 Step ${idx + 1} 失败: ${e.message}`);
    }

    // 等待 delay
    if (step.delay && step.delay > 0) {
      await this._sleep(step.delay);
    }
  }

  _scheduleNextStep() {
    if (!this.playbackState.isPlaying) return;

    const state = this.playbackState;
    state.timerId = setTimeout(async () => {
      await this._playSequence();
    }, 10);
  }

  async _playSequence() {
    const state = this.playbackState;
    if (!state.isPlaying) return;

    const steps = state.steps;

    if (state.currentStepIndex >= steps.length) {
      // 检查 UI 循环
      if (state.uiLoop) {
        state.currentStepIndex = 0;
        this._log('info', '循环播放: 从头开始');
        this._scheduleNextStep();
        return;
      }

      // 检查 JSON 内部循环
      const jsonLoop = state.jsonLoop;
      if (jsonLoop?.enabled && !state.loopState) {
        state.loopState = {
          start: jsonLoop.startIndex ?? 0,
          end: jsonLoop.endIndex ?? steps.length - 1,
          remaining: jsonLoop.count ?? 1,
          delayBetween: jsonLoop.delayBetweenLoops ?? 0,
          inLoop: false,
        };
      }

      if (state.loopState) {
        state.loopState.remaining--;
        if (state.loopState.remaining >= 0) {
          state.currentStepIndex = state.loopState.start;
          if (state.loopState.delayBetween > 0) {
            await this._sleep(state.loopState.delayBetween);
          }
          this._log('info', `循环: 第${state.loopState.remaining + 1}轮`);
          this._scheduleNextStep();
          return;
        }
      }

      // 播放完全结束
      this._handleActionStop();
      this._log('info', '播放完成');
      this.broadcast('action:playbackProgress', {
        currentStep: steps.length,
        totalSteps: steps.length,
        status: 'completed',
      });
      return;
    }

    await this._executeStep(state.currentStepIndex);
    state.currentStepIndex++;
    this._scheduleNextStep();
  }

  // ==================== 自定义控件发送 ====================
  async _handleControlSend(data, action) {
    this._log('info', `[控件] ${action}: ${data.data || '(空)'}`);
    if (!this.ble.globalTxChar) {
      this._log('warn', '未绑定全局发送特征，控件数据无法发送');
      return;
    }

    try {
      await this.ble.writeViaGlobalTx(data.data || '', data.hex !== false);
    } catch (e) {
      this._log('error', `控件发送失败: ${e.message}`);
    }
  }

  // ==================== 底层发送 ====================
  async _sendPacket(packet, description) {
    const hexStr = toHexString(packet);
    this._log('info', `[发送] ${description}: ${hexStr}`);

    if (this.ble.globalTxChar) {
      try {
        await this.ble.writeCharacteristic(
          this.ble.globalTxChar.serviceUuid,
          this.ble.globalTxChar.charUuid,
          hexStr,
          true
        );
      } catch (e) {
        this._log('error', `发送失败: ${e.message}`);
      }
    }
  }

  // ==================== 辅助方法 ====================
  _log(level, message) {
    this.broadcast('log:message', { level, message });
  }

  _sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * 停止 WS 服务器
   */
  async stop() {
    // 停止播放
    this._handleActionStop();

    // 断开所有客户端
    for (const ws of this.clients) {
      ws.close();
    }

    if (this.wss) {
      await new Promise(resolve => this.wss.close(resolve));
    }
  }
}

module.exports = WSServer;
