/**
 * 数据持久化模块
 * 使用 electron-store 保存控件布局、动作组、全局绑定等
 */

const path = require('path');
const fs = require('fs');

class Storage {
  constructor(appDataPath) {
    this.dataPath = appDataPath || path.join(
      __dirname, '..', 'data'
    );

    // 确保数据目录存在
    if (!fs.existsSync(this.dataPath)) {
      fs.mkdirSync(this.dataPath, { recursive: true });
    }

    this._cache = {};
    this._loadAll();
  }

  _getFilePath(key) {
    return path.join(this.dataPath, `${key}.json`);
  }

  _loadAll() {
    try {
      const files = fs.readdirSync(this.dataPath);
      for (const file of files) {
        if (file.endsWith('.json')) {
          const key = file.replace('.json', '');
          try {
            const content = fs.readFileSync(path.join(this.dataPath, file), 'utf-8');
            this._cache[key] = JSON.parse(content);
          } catch (e) {
            console.error(`[Storage] 加载 ${key} 失败:`, e.message);
          }
        }
      }
    } catch (e) {
      console.error('[Storage] 初始化失败:', e.message);
    }
  }

  async save(key, value) {
    this._cache[key] = value;
    const filePath = this._getFilePath(key);
    try {
      fs.writeFileSync(filePath, JSON.stringify(value, null, 2), 'utf-8');
      return true;
    } catch (e) {
      console.error(`[Storage] 保存 ${key} 失败:`, e.message);
      return false;
    }
  }

  async load(key) {
    return this._cache[key] || null;
  }

  async delete(key) {
    delete this._cache[key];
    const filePath = this._getFilePath(key);
    try {
      if (fs.existsSync(filePath)) {
        fs.unlinkSync(filePath);
      }
      return true;
    } catch (e) {
      console.error(`[Storage] 删除 ${key} 失败:`, e.message);
      return false;
    }
  }

  async listKeys() {
    return Object.keys(this._cache);
  }
}

module.exports = Storage;
