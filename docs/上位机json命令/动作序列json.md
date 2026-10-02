## 上位机JSON 命令示例

### 1.单个静态动作

```json
{
  "name": "静态姿势",
  "version": 2,
  "steps": [
    {
      "servos": [
        { "id": 1, "position": 500, "time": 0 },
        { "id": 2, "position": 620, "time": 0 },
        { "id": 3, "position": 480, "time": 0 }
      ],
      "delay": 200
    }
  ]
}
```

- `delay` 字段**可选**，默认值为 0，解析时若不存在则按 0 处理。
- 用户若不需要延时，可以省略 `delay` 或设为 0。

### 2. 基础线性序列（无循环/跳转）

每个动作序列包含一个 `steps` 数组，每个 step 对应一个静态动作组（即一条 `0x02` 指令）。

```json
{
  "name": "前进摆动",
  "version": 3,
  "steps": [
    {
      "servos": [
        { "id": 1, "position": 500, "time": 200 },
        { "id": 2, "position": 600, "time": 200 },
        { "id": 3, "position": 500, "time": 200 }
      ],
      "delay": 200
    },
    {
      "servos": [
        { "id": 1, "position": 600, "time": 300 },
        { "id": 2, "position": 500, "time": 300 },
        { "id": 3, "position": 600, "time": 300 }
      ],
      "delay": 300
    },
    {
      "servos": [
        { "id": 1, "position": 800, "time": 300 },
        { "id": 2, "position": 400, "time": 300 },
        { "id": 3, "position": 800, "time": 300 }
      ],
      "delay": 300
    }
  ]
}
```

- `servos` 数组中每个元素为 `[舵机ID, 目标位置脉冲(0~1000), 运行时间(ms)]`。
- 用户自定义每个指令发送的时间间隔`delayX`
- 没有`control`类型步骤，默认动作序列只执行一次

### 3. 带控制指令的序列（支持循环、等待、跳转）

若需在 JSON 内直接表达循环逻辑（例如重复执行某几个步骤），可增加 `control` 类型步骤。

```json
{
  "name": "前进摆动",
  "version": 3,
  "steps": [
    {
      "servos": [
        { "id": 1, "position": 500, "time": 200 },
        { "id": 2, "position": 600, "time": 200 },
        { "id": 3, "position": 500, "time": 200 }
      ],
      "delay": 200
    },
    {
      "servos": [
        { "id": 1, "position": 600, "time": 300 },
        { "id": 2, "position": 500, "time": 300 },
        { "id": 3, "position": 600, "time": 300 }
      ],
      "delay": 300
    },
    {
      "servos": [
        { "id": 1, "position": 800, "time": 300 },
        { "id": 2, "position": 400, "time": 300 },
        { "id": 3, "position": 800, "time": 300 }
      ],
      "delay": 300
    }
  ],
  "playback": {
    "loop": {
      "enabled": true,
      "count": 5,
      "startIndex": 0,
      "endIndex": 1,
      "delayBetweenLoops": 500
    }
  }
}
```

**字段说明**

| 字段                              | 类型    | 默认值   | 说明                                                         |
| :-------------------------------- | :------ | :------- | :----------------------------------------------------------- |
| `playback.loop.enabled`           | boolean | `false`  | 是否启用循环                                                 |
| `playback.loop.count`             | integer | 1        | 循环次数：`-1` 表示无限循环；正整数表示有限次数（包括第一次）。例如 `count=3` 表示循环区间执行 3 次。 |
| `playback.loop.startIndex`        | integer | 0        | 循环起始 step 索引（0‑based），包含。                        |
| `playback.loop.endIndex`          | integer | 最后一帧 | 循环结束 step 索引（包含）。                                 |
| `playback.loop.delayBetweenLoops` | integer | 0        | 每轮循环结束后（即执行完 `endIndex` 对应的 step 后）额外等待的毫秒数，再开始下一轮循环。 |

### 4.关于嵌套循环

如果你有一系列的动作组，动作组之间有 某一段需要循环，上面的方式二已经可以满足需求

所谓的嵌套循环就是，你进入了 某几个循环的动作组

但是 在循环内部，你想继续 再循环某几个动作 线性执行...

算了，手动展开吧，也就是多复制粘贴几次的事

因为内部循环不可能是无限循环。
