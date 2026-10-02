# host — 调试上位机

"不用写一行代码进行调蛇"的桌面调试工具：可自定义步态，自由设置正弦运动的摆幅、波速等参数，内置舵机校准、遥控布局、动作组编舞等功能。至此，调试蛇步态无需再区分"电控组"与"结构组"。

## 架构说明

本软件采用 **Web 技术做前端**（浏览器页面 + 本地 Node 后端），优点是可以适配 Windows / macOS / Linux 等不同系统，缺点是运行占用资源较高。后端通过 BLE（noble）与下位机通信，页面通过 WebSocket 与后端通信。

## 运行

前置要求：[Node.js](https://nodejs.org) 16+。

- Windows：双击 `start.bat`
- Linux / macOS：`./start.sh`
- 或手动执行：

```bash
npm install
npm start
```

启动后会自动打开浏览器进入主界面。

> **BLE 说明**：BLE 依赖 `@abandonware/noble`（已声明为可选依赖）。Windows 10 (≥15063) / macOS 会自动使用预编译绑定；若安装失败，程序仍可启动并进入**模拟模式**（可体验界面，但无法连接真实设备）。Linux 用户需要 `libbluetooth-dev` 等系统头文件。

## 界面与配套文档

| 功能 | 相关文档 |
| --- | --- |
| 运动控制（初级遥控） | [docs/字符串通信协议](../docs/字符串通信协议/) |
| 舵机校准（自动/手动） | 见项目介绍视频 |
| 二进制高速通道（第三、四界面） | [docs/二进制通信协议](../docs/二进制通信协议/) |
| 动作组编舞（0 代码） | [docs/上位机json命令](../docs/上位机json命令/) |

仓库内置的示例遥控布局（[layout/control-layout.json](layout/control-layout.json)）可在上位机第二个界面快速导入。

## 目录结构

```
host/
├── server.js        # 本地后端入口（HTTP + WebSocket）
├── src/
│   ├── ble-manager.js   # BLE 扫描/连接/GATT（无 noble 时自动模拟模式）
│   ├── protocol.js      # 数据包解析
│   ├── ws-server.js     # WebSocket 服务
│   └── storage.js       # 本地数据存储
├── html/            # 前端页面（框架 + 各功能界面）
├── data/            # 运行时数据（BLE 设备记录等，自动生成，不入库）
├── start.bat / start.sh
└── package.json
```
