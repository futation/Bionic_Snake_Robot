/**
 * 纯 Node.js 后端服务器
 * 不依赖 Electron API，可独立运行
 * 支持 pkg 打包为单文件 .exe
 * 负责：WebSocket IPC、BLE 管理、协议处理、数据持久化
 */

const http = require('http');
const fs = require('fs');
const path = require('path');

// 检测是否在 pkg 打包环境中运行
const isPkg = !!(process.pkg);

// 后端模块
const BLEManager = require('./src/ble-manager');
const WSServer = require('./src/ws-server');
const Storage = require('./src/storage');

let wsServer = null;
let bleManager = null;
let storage = null;
let httpServer = null;

async function start(port = 0) {
  // 1. 初始化存储（pkg 模式下数据存到 exe 同目录）
  const dataDir = isPkg
    ? path.join(path.dirname(process.execPath), 'data')
    : undefined;
  storage = new Storage(dataDir);

  // 2. 初始化 BLE 管理器（模拟模式）
  bleManager = new BLEManager({ simulationMode: true });

  // 3. 创建 HTTP + WebSocket 服务器共用端口
  httpServer = http.createServer((req, res) => {
    // 提供静态文件服务（让 Electron 能直接加载 HTML）
    let filePath = req.url === '/' ? '/html/framework.html' : req.url;
    filePath = path.join(__dirname, filePath.replace(/^\//, ''));

    const extMap = {
      '.html': 'text/html; charset=utf-8',
      '.js': 'application/javascript',
      '.css': 'text/css',
      '.json': 'application/json',
    };
    const ext = path.extname(filePath);
    const contentType = extMap[ext] || 'text/plain';

    fs.readFile(filePath, (err, data) => {
      if (err) {
        res.writeHead(404);
        res.end('Not Found');
        return;
      }
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(data);
    });
  });

  // 4. 先启动 HTTP 服务器，再挂载 WebSocket
  return new Promise((resolve) => {
    httpServer.listen(port, '127.0.0.1', async () => {
      const addr = httpServer.address();
      
      // WebSocket 挂载到 HTTP 服务器
      wsServer = new WSServer(bleManager, storage);
      await wsServer.startOnServer(httpServer);
      
      console.log(`[Server] HTTP + WebSocket 服务器运行在 http://127.0.0.1:${addr.port}`);
      console.log(`[Server] 前端地址: http://127.0.0.1:${addr.port}/html/framework.html`);
      resolve({ port: addr.port, httpServer, wsServer });
    });
  });
}

async function stop() {
  if (wsServer) await wsServer.stop();
  if (httpServer) await new Promise(r => httpServer.close(r));
  console.log('[Server] 已停止');
}

// 直接运行时启动服务器
if (require.main === module) {
  start().then(({ port }) => {
    const url = `http://127.0.0.1:${port}/html/framework.html`;
    console.log(`\n========================================`);
    console.log(`  蛇形机器人调试上位机 - 后端服务`);
    console.log(`  地址: ${url}`);
    console.log(`========================================\n`);
    console.log('正在打开浏览器...');

    // 自动打开浏览器
    const { exec } = require('child_process');
    const cmd = process.platform === 'win32'
      ? `start "" "${url}"`
      : process.platform === 'darwin'
        ? `open "${url}"`
        : `xdg-open "${url}"`;
    exec(cmd, (err) => {
      if (err) console.log('请手动打开: ' + url);
    });
  }).catch(err => {
    console.error('启动失败:', err);
    process.exit(1);
  });
}

module.exports = { start, stop };
