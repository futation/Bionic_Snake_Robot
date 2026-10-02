/**
 * BLE 管理器模块
 * 支持真实 BLE (noble) 和模拟模式
 * 负责扫描、连接、GATT操作、全局绑定
 */

const EventEmitter = require('events');
const { PacketParser, toHexString } = require('./protocol');

// 尝试加载 noble，若不可用则使用模拟模式
let noble = null;
try {
  noble = require('@abandonware/noble');
} catch (e) {
  // noble 不可用，将使用模拟模式
}

class BLEManager extends EventEmitter {
  constructor(options = {}) {
    super();
    this.simulationMode = options.simulationMode || (noble === null);
    this.isScanning = false;
    this.connectedDevice = null;
    this.connectedPeripheral = null;
    this.services = {};
    this.characteristics = {};

    // 全局绑定
    this.globalTxChar = null;  // { serviceUuid, charUuid }
    this.globalRxChar = null;  // { serviceUuid, charUuid }

    // 通知订阅
    this.subscriptions = new Map(); // charUuid -> callback

    // 数据流解析器（用于全局接收特征）
    this.dataParser = new PacketParser();
    this.dataParser.onError = (msg) => {
      this.emit('log', { level: 'error', message: `[协议解析] ${msg}` });
    };

    // 模拟设备数据
    this._mockDevices = [];
    this._mockScanTimer = null;
    this._mockNotifyTimer = null;

    if (!this.simulationMode && noble) {
      this._setupNoble();
    }
  }

  _setupNoble() {
    noble.on('stateChange', (state) => {
      this.emit('log', { level: 'info', message: `BLE 适配器状态: ${state}` });
      if (state === 'poweredOn') {
        this.emit('status', { adapterInfo: '蓝牙适配器: 就绪' });
      } else {
        this.emit('status', { adapterInfo: `蓝牙适配器: ${state}` });
      }
    });

    noble.on('discover', (peripheral) => {
      const dev = {
        name: peripheral.advertisement.localName || 'N/A',
        mac: peripheral.address || peripheral.id,
        rssi: peripheral.rssi,
        delay: 0,
        peripheral: peripheral,
      };
      this.emit('deviceDiscovered', dev);
    });

    noble.on('scanStop', () => {
      this.isScanning = false;
      this.emit('log', { level: 'info', message: 'BLE 扫描已停止' });
    });
  }

  // ==================== 扫描 ====================
  async startScan() {
    if (this.isScanning) return;
    this.isScanning = true;

    if (this.simulationMode) {
      this._startMockScan();
    } else {
      await noble.startScanningAsync([], true);
    }
    this.emit('log', { level: 'info', message: '开始扫描 BLE 设备...' });
  }

  async stopScan() {
    if (!this.isScanning) return;
    this.isScanning = false;

    if (this.simulationMode) {
      if (this._mockScanTimer) clearInterval(this._mockScanTimer);
    } else {
      await noble.stopScanningAsync();
    }
    this.emit('log', { level: 'info', message: '停止扫描 BLE 设备' });
  }

  _startMockScan() {
    // 模拟模式：不生成假设备（扫描由前端 Web Bluetooth 处理）
    this._mockDevices = [];
    this.emit('log', { level: 'info', message: '请在浏览器弹出的蓝牙选择器中选取设备' });
  }

  // ==================== 连接 ====================
  async connect(mac) {
    if (this.connectedDevice) {
      await this.disconnect();
    }

    let device;
    if (this.simulationMode) {
      device = this._mockDevices.find(d => d.mac === mac);
      if (!device) {
        device = { name: 'SnakeRobot', mac, rssi: -50, delay: 50 };
      }
    } else {
      // 在实际实现中，这里通过 noble 连接
      // const peripheral = ...; await peripheral.connectAsync();
      throw new Error('真实 BLE 连接暂未实现，请安装 @abandonware/noble');
    }

    this.connectedDevice = device;

    // 模拟服务发现
    this._buildMockGatt();

    this.emit('log', { level: 'info', message: `已连接设备: ${device.name} (${device.mac})` });
    this.emit('connected', device);
    this.emit('status', {
      scanInfo: `已停止 | 设备:${this._mockDevices.length} | 连接:1`,
      adapterInfo: `已连接 [${device.name}]`,
    });

    return device;
  }

  async disconnect() {
    if (!this.connectedDevice) return;

    // 取消所有订阅
    if (this._mockNotifyTimer) clearInterval(this._mockNotifyTimer);
    this.subscriptions.clear();

    // 清除全局绑定
    const hadTx = !!this.globalTxChar;
    const hadRx = !!this.globalRxChar;
    this.globalTxChar = null;
    this.globalRxChar = null;

    this.emit('log', { level: 'info', message: `已断开连接: ${this.connectedDevice.name}` });
    if (hadTx || hadRx) {
      this.emit('log', { level: 'info', message: '全局绑定已自动清除' });
      this.emit('globalBindUpdated', { txChar: null, rxChar: null });
    }

    this.connectedDevice = null;
    this.connectedPeripheral = null;
    this.services = {};
    this.characteristics = {};
    this.dataParser.reset();

    this.emit('disconnected');
    this.emit('status', {
      scanInfo: `已停止 | 设备:${this._mockDevices.length} | 连接:0`,
      adapterInfo: '蓝牙适配器: 未连接',
    });
  }

  // ==================== 模拟 GATT ====================
  _buildMockGatt() {
    this.services = {
      '00001800-0000-1000-8000-00805f9b34fb': {
        name: 'Generic Access',
        characteristics: [
          { uuid: '00002a00-0000-1000-8000-00805f9b34fb', name: 'Device Name', properties: ['read'] },
          { uuid: '00002a01-0000-1000-8000-00805f9b34fb', name: 'Appearance', properties: ['read'] },
        ],
      },
      '4fafc201-1fb5-459e-8fcc-c5c9c331914b': {
        name: 'Serial Service',
        characteristics: [
          {
            uuid: '1b9a473a-4493-4536-8b2b-9d4133488256',
            name: 'TX Characteristic',
            properties: ['write', 'writeWithoutResponse'],
          },
          {
            uuid: '1b9a473a-4493-4536-8b2b-9d4133488257',
            name: 'RX Characteristic',
            properties: ['read', 'notify'],
          },
        ],
      },
      '0000ffe0-0000-1000-8000-00805f9b34fb': {
        name: 'Custom Service',
        characteristics: [
          { uuid: '0000ffe1-0000-1000-8000-00805f9b34fb', name: 'Custom Char 1', properties: ['write', 'notify'] },
        ],
      },
    };

    // 构建特征查找表
    this.characteristics = {};
    for (const [sUuid, service] of Object.entries(this.services)) {
      for (const char of service.characteristics) {
        this.characteristics[char.uuid] = { ...char, serviceUuid: sUuid };
      }
    }
  }

  // ==================== 特征操作 ====================
  async writeCharacteristic(serviceUuid, charUuid, data, hex = true) {
    if (!this.connectedDevice) {
      throw new Error('未连接设备');
    }

    let buffer;
    if (hex) {
      const cleaned = data.replace(/\s+/g, '');
      buffer = Buffer.from(cleaned, 'hex');
    } else {
      buffer = Buffer.from(data, 'utf-8');
    }

    if (this.simulationMode) {
      this.emit('log', {
        level: 'info',
        message: `[Write] ${charUuid.substring(0, 8)}... 数据: ${toHexString(buffer)}`,
      });
      return;
    }

    // 真实写入
    // const char = this.characteristics[charUuid];
    // await char.writeAsync(buffer, char.properties.includes('writeWithoutResponse'));
    throw new Error('真实 BLE 写入暂未实现');
  }

  async readCharacteristic(serviceUuid, charUuid) {
    if (!this.connectedDevice) {
      throw new Error('未连接设备');
    }

    if (this.simulationMode) {
      const mockData = Buffer.from([0x48, 0x65, 0x6C, 0x6C, 0x6F]); // "Hello"
      this.emit('dataReceived', {
        serviceUuid,
        charUuid,
        data: toHexString(mockData),
        hex: true,
      });
      return mockData;
    }

    throw new Error('真实 BLE 读取暂未实现');
  }

  async subscribeCharacteristic(serviceUuid, charUuid) {
    if (!this.connectedDevice) {
      throw new Error('未连接设备');
    }

    const key = `${serviceUuid}:${charUuid}`;

    if (this.simulationMode) {
      this.emit('log', { level: 'info', message: `订阅通知: ${charUuid.substring(0, 8)}...` });

      // 模拟定期通知
      if (this._mockNotifyTimer) clearInterval(this._mockNotifyTimer);
      this._mockNotifyTimer = setInterval(() => {
        const randomHex = Array.from({ length: 4 }, () =>
          Math.floor(Math.random() * 256).toString(16).padStart(2, '0')
        ).join(' ');
        const data = Buffer.from(randomHex.replace(/\s+/g, ''), 'hex');

        // 如果已绑定为全局接收特征，通过协议解析器处理
        if (this.globalRxChar && this.globalRxChar.charUuid === charUuid) {
          this.emit('globalDataReceived', data);
        }

        this.emit('dataReceived', {
          serviceUuid,
          charUuid,
          data: randomHex,
          hex: true,
        });
      }, 3000);
    }

    this.subscriptions.set(key, true);
    this.emit('log', { level: 'info', message: `已订阅特征通知` });
  }

  async unsubscribeCharacteristic(serviceUuid, charUuid) {
    const key = `${serviceUuid}:${charUuid}`;
    this.subscriptions.delete(key);

    if (this._mockNotifyTimer) {
      clearInterval(this._mockNotifyTimer);
      this._mockNotifyTimer = null;
    }

    this.emit('log', { level: 'info', message: `已取消订阅特征通知` });
  }

  // ==================== 全局绑定 ====================
  bindTxChar(serviceUuid, charUuid) {
    if (this.globalTxChar) {
      this.emit('log', { level: 'warn', message: '已有全局发送绑定，请先取消' });
      return false;
    }

    const char = this.characteristics[charUuid];
    if (!char || !char.properties.some(p => p.includes('write'))) {
      this.emit('log', { level: 'warn', message: '该特征不支持写入' });
      return false;
    }

    this.globalTxChar = { serviceUuid, charUuid };
    this.emit('log', {
      level: 'info',
      message: `已绑定全局发送特征: ${charUuid.substring(0, 8)}...`,
    });
    this.emit('globalBindUpdated', {
      txChar: this.globalTxChar,
      rxChar: this.globalRxChar,
    });
    return true;
  }

  bindRxChar(serviceUuid, charUuid) {
    if (this.globalRxChar) {
      this.emit('log', { level: 'warn', message: '已有全局接收绑定，请先取消' });
      return false;
    }

    const char = this.characteristics[charUuid];
    if (!char || !char.properties.some(p => p === 'notify' || p === 'indicate')) {
      this.emit('log', { level: 'warn', message: '该特征不支持通知/指示' });
      return false;
    }

    this.globalRxChar = { serviceUuid, charUuid };
    this.emit('log', {
      level: 'info',
      message: `已绑定全局接收特征: ${charUuid.substring(0, 8)}...`,
    });
    this.emit('globalBindUpdated', {
      txChar: this.globalTxChar,
      rxChar: this.globalRxChar,
    });

    // 自动订阅
    this.subscribeCharacteristic(serviceUuid, charUuid);
    return true;
  }

  unbindTxChar() {
    if (this.globalTxChar) {
      this.emit('log', { level: 'info', message: '已取消全局发送绑定' });
      this.globalTxChar = null;
      this.emit('globalBindUpdated', { txChar: null, rxChar: this.globalRxChar });
    }
  }

  unbindRxChar() {
    if (this.globalRxChar) {
      this.emit('log', { level: 'info', message: '已取消全局接收绑定' });
      this.globalRxChar = null;
      this.emit('globalBindUpdated', { txChar: this.globalTxChar, rxChar: null });
    }
  }

  cancelAllBindings() {
    const hadTx = !!this.globalTxChar;
    const hadRx = !!this.globalRxChar;
    this.globalTxChar = null;
    this.globalRxChar = null;
    if (hadTx || hadRx) {
      this.emit('log', { level: 'info', message: '所有全局绑定已取消' });
      this.emit('globalBindUpdated', { txChar: null, rxChar: null });
    }
  }

  /**
   * 通过全局发送特征写入数据
   */
  async writeViaGlobalTx(data, hex = true) {
    if (!this.globalTxChar) {
      throw new Error('未绑定全局发送特征');
    }
    return this.writeCharacteristic(
      this.globalTxChar.serviceUuid,
      this.globalTxChar.charUuid,
      data,
      hex
    );
  }
}

module.exports = BLEManager;
