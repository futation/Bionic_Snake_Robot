/**
 * 通信协议解析与命令构造模块
 * 包格式: 0xAA | Cmd | Len | Payload... | CS | 0x55
 * 校验和: CS = 0xFF - (Cmd + Len + Payload每个字节) & 0xFF
 */

// 命令字常量
const CMD = {
  // 校准相关
  ADJUST_SERVO:     0x01,  // 实时调整单舵机
  SAVE_SERVO:       0x03,  // 保存到EEPROM
  FEEDBACK:         0x04,  // 通用操作反馈
  ENTER_CALIBRATION:0x05,  // 进入校准模式
  SERVO_PARAMS:     0x06,  // 下位机上报舵机参数
  CHANGE_ID:        0x07,  // 修改舵机ID
  READ_PARAMS:      0x08,  // 读取参数
  EXIT_CALIBRATION: 0x09,  // 退出校准模式

  // 动作组相关
  ENTER_ACTION:     0x0A,  // 进入动作组模式
  EXIT_ACTION:      0x0B,  // 退出动作组模式
  SCAN_POSITIONS:   0x0C,  // 扫描舵机位置
  SCAN_RESULT:      0x0D,  // 上报扫描结果
  POWER_OFF:        0x0E,  // 舵机掉电
  ACTION_STEP:      0x02,  // 执行静态动作组
};

// 反馈状态码含义
const FEEDBACK_CODES = {
  0x01: '参数读取成功',
  0x02: '参数保存成功',
  0x03: 'ID修改成功',
  0x10: '进入手动校准模式成功',
  0x11: '退出手动校准模式成功',
  0x12: '进入动作组模式成功',
  0x13: '退出动作组模式成功',
  0x14: '舵机掉电成功',
  0xFF: '操作失败',
};

// 反馈原因码
const ERROR_CODES = {
  0x01: '校验和/格式错误',
  0x02: '参数无效',
  0x03: '执行超时/舵机无响应',
  0x04: '模式不匹配',
  0x05: '硬件状态不允许',
  0x06: '舵机数量为零',
};

/**
 * 计算校验和
 * @param {number} cmd
 * @param {number} len
 * @param {number[]} payload
 * @returns {number}
 */
function checksum(cmd, len, payload) {
  let sum = (cmd + len) & 0xFF;
  for (let b of payload) {
    sum = (sum + b) & 0xFF;
  }
  return (0xFF - sum) & 0xFF;
}

/**
 * 构造二进制命令包
 * @param {number} cmd - 命令字
 * @param {number[]} payloadBytes - payload 字节数组
 * @returns {Buffer}
 */
function buildPacket(cmd, payloadBytes = []) {
  const len = payloadBytes.length;
  const cs = checksum(cmd, len, payloadBytes);
  return Buffer.from([0xAA, cmd, len, ...payloadBytes, cs, 0x55]);
}

/**
 * 将 Buffer 转为 HEX 字符串（用于日志）
 * @param {Buffer} buf
 * @returns {string}
 */
function toHexString(buf) {
  return Array.from(buf)
    .map(b => b.toString(16).toUpperCase().padStart(2, '0'))
    .join(' ');
}

/**
 * 将空格分隔的 HEX 字符串解析为 Buffer
 * @param {string} hexStr
 * @returns {Buffer}
 */
function fromHexString(hexStr) {
  const cleaned = hexStr.replace(/\s+/g, '');
  const bytes = [];
  for (let i = 0; i < cleaned.length; i += 2) {
    bytes.push(parseInt(cleaned.substring(i, i + 2), 16));
  }
  return Buffer.from(bytes);
}

/**
 * 数据包解析器 - 从流式数据中提取完整包
 */
class PacketParser {
  constructor() {
    this.buffer = Buffer.alloc(0);
    this.onPacket = null;
    this.onError = null;
  }

  /**
   * 输入数据，尝试解析包
   * @param {Buffer} data
   * @returns {Array<{cmd: number, payload: Buffer}>}
   */
  feed(data) {
    this.buffer = Buffer.concat([this.buffer, data]);
    const packets = [];

    while (this.buffer.length >= 6) { // 最小包: AA cmd len cs 55 = 5 + payload最少0
      // 寻找包头 0xAA
      const startIdx = this.buffer.indexOf(0xAA);
      if (startIdx === -1) {
        this.buffer = Buffer.alloc(0);
        break;
      }

      // 跳过无效数据
      if (startIdx > 0) {
        this.buffer = this.buffer.slice(startIdx);
      }

      if (this.buffer.length < 6) break;

      const cmd = this.buffer[1];
      const len = this.buffer[2];

      const totalLen = 5 + len; // AA + cmd + len + payload(len) + cs + 55 = 5 + len
      if (this.buffer.length < totalLen) break;

      // 检查包尾 0x55
      if (this.buffer[totalLen - 1] !== 0x55) {
        this.buffer = this.buffer.slice(1); // 跳过这个AA
        continue;
      }

      const payload = this.buffer.slice(3, 3 + len);
      const receivedCs = this.buffer[3 + len];

      // 验算校验和
      const expectedCs = checksum(cmd, len, Array.from(payload));
      if (receivedCs !== expectedCs) {
        this.onError && this.onError(`校验和错误: 收到 0x${receivedCs.toString(16)}, 期望 0x${expectedCs.toString(16)}`);
        this.buffer = this.buffer.slice(totalLen);
        continue;
      }

      packets.push({ cmd, payload });

      // 移除已解析的包
      this.buffer = this.buffer.slice(totalLen);
    }

    return packets;
  }

  reset() {
    this.buffer = Buffer.alloc(0);
  }
}

/**
 * 构造 0x01 - 实时调整舵机
 */
function buildAdjustServo(servoId, position, offset) {
  const posLow = position & 0xFF;
  const posHigh = (position >> 8) & 0xFF;
  const offsetByte = offset >= 0 ? offset & 0xFF : (256 + offset) & 0xFF;
  return buildPacket(CMD.ADJUST_SERVO, [servoId, posLow, posHigh, offsetByte]);
}

/**
 * 构造 0x03 - 保存舵机参数到EEPROM
 */
function buildSaveServo(servoId, position, offset) {
  const posLow = position & 0xFF;
  const posHigh = (position >> 8) & 0xFF;
  const offsetByte = offset >= 0 ? offset & 0xFF : (256 + offset) & 0xFF;
  return buildPacket(CMD.SAVE_SERVO, [servoId, posLow, posHigh, offsetByte]);
}

/**
 * 构造 0x07 - 修改舵机ID
 */
function buildChangeId(oldId, newId) {
  return buildPacket(CMD.CHANGE_ID, [oldId, newId]);
}

/**
 * 构造 0x02 - 动作组指令
 * @param {Array<{id: number, position: number, time: number}>} servos
 */
function buildActionStep(servos) {
  const payload = [];
  for (const s of servos) {
    payload.push(s.id);
    payload.push(s.position & 0xFF);
    payload.push((s.position >> 8) & 0xFF);
    payload.push(s.time & 0xFF);
    payload.push((s.time >> 8) & 0xFF);
  }
  return buildPacket(CMD.ACTION_STEP, payload);
}

/**
 * 构造无 payload 命令
 */
function buildSimpleCmd(cmd) {
  return buildPacket(cmd, []);
}

/**
 * 解析 0x04 反馈包
 * @returns {{code: number, reason: number|null, message: string}}
 */
function parseFeedback(payload) {
  const code = payload[0];
  const reason = payload.length > 1 ? payload[1] : null;
  let message = FEEDBACK_CODES[code] || `未知状态码: 0x${code.toString(16)}`;
  if (reason !== null && code === 0xFF) {
    message += ` (原因: ${ERROR_CODES[reason] || '未知'})`;
  }
  return { code, reason, message };
}

/**
 * 解析 0x06 - 舵机参数列表
 * @returns {Array<{id: number, position: number, offset: number}>}
 */
function parseServoParams(payload) {
  const servos = [];
  for (let i = 0; i < payload.length; i += 4) {
    if (i + 3 >= payload.length) break;
    const id = payload[i];
    const position = payload[i + 1] | (payload[i + 2] << 8);
    let offset = payload[i + 3];
    if (offset > 127) offset = offset - 256; // 有符号数转换
    servos.push({ id, position, offset });
  }
  return servos;
}

/**
 * 解析 0x0D - 扫描结果
 * @returns {Array<{id: number, position: number}>}
 */
function parseScanResult(payload) {
  const positions = [];
  for (let i = 0; i < payload.length; i += 3) {
    if (i + 2 >= payload.length) break;
    const id = payload[i];
    const position = payload[i + 1] | (payload[i + 2] << 8);
    positions.push({ id, position });
  }
  return positions;
}

module.exports = {
  CMD,
  FEEDBACK_CODES,
  ERROR_CODES,
  checksum,
  buildPacket,
  toHexString,
  fromHexString,
  PacketParser,
  buildAdjustServo,
  buildSaveServo,
  buildChangeId,
  buildActionStep,
  buildSimpleCmd,
  parseFeedback,
  parseServoParams,
  parseScanResult,
};
