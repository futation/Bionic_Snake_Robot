# hardware — 舵机驱动板 Bus-Driver-Neo

自研蛇形机器人舵机驱动板（嘉立创 EDA 设计，PCB 版本 bus-linkV2.1），板载 ESP32-S3 主控与总线舵机接口，适配幻尔（Hiwonder）总线舵机。

## 文件说明

| 文件 | 用途 |
| --- | --- |
| `Bus-LinkV2.1.epro2` | 嘉立创 EDA Pro 工程文件，供想学习/修改 PCB 设计的同学使用 |
| `Gerber_bus-linkV2.1_2026-06-18.zip` | Gerber 制板文件，可直接下单 PCB |
| `BOM_Bus-LinkV2.1_bus-linkV2.1_2026-06-18.xlsx` | 物料清单（PCB 上的元件），用于下单或直接 SMT |
| `3D_bus-linkV2.1_2026-06-18.step` | 3D 模型，供结构组同学做参考 |
| `test_code/` | 焊接验证测试代码 |

## 下单与生产

Gerber + BOM 两个文件即可完成 PCB 下单或直接 SMT 贴片。

**关于焊接**：不推荐手动焊接这块板子，建议直接 SMT。板载经过验证没有问题；手动焊接失败有一定的危险性，而且浪费材料，每个人学习上手的成本也高，相比起来非常不划算。

## 焊接验证

焊接完成后，将 `test_code/SerialServoRP/` 烧入主控，接上幻尔总线舵机上电：舵机有转动说明没有问题。**注意两个舵机接口都要测试一下。**

验证通过后，烧录正式固件见 [firmware/](../firmware/README.md)。
