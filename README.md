# ESP32-S3 + STM32F407 双 MCU OTA 升级系统

基于 ESP32-S3（WiFi 中转）与 STM32F407（Bootloader + APP）的双 MCU 架构，实现从 OneNET 云端到 MCU 闪存的端到端固件升级，具备三层 CRC 校验与防变砖回滚机制。

## 架构总览

```
OneNET 云平台
     │  HTTPS (固件 + 元数据)
     ▼
ESP32-S3 ──── WiFi 连接 / MQTT 通信 / OTA 下载
     │  UART 自定义协议 (0x66...0xBB)
     ▼
STM32F407 ──── Bootloader (升级控制) + APP (业务逻辑)
              A/B 双分区 Flash 布局
```

| 模块 | 芯片 | 职责 |
|------|------|------|
| 云端通信 | ESP32-S3 | OneNET MQTT 订阅、HTTPS OTA 下载、SPIFFS 存储 |
| 协议中转 | ESP32-S3 | 串口组帧/解帧、逐帧 CRC、固件分发 |
| Bootloader | STM32F407 | 接收固件、Flash 擦写、端到端 CRC 校验、分区跳转 |
| APP | STM32F407 | FreeRTOS + LVGL 温湿度控制系统业务逻辑 |

## 核心特性

### 三层 CRC 校验防线

| 层级 | 校验对象 | 作用 |
|------|----------|------|
| 传输层 | 每个数据帧 | 检出 UART 传输过程中的位翻转 |
| 命令层 | 信息帧自校验 | 防止固件元数据（大小/版本/CRC）被篡改或错位 |
| 落盘层 | 整体固件 CRC32 | 检出下载落盘或 Flash 擦写引入的错误 |

> 流式 CRC32 采用多项式 `0xEDB88320`，init/final 异或 `0xFFFFFFFF`，下载时逐帧累加，无需二次读盘。

### 防变砖策略

- 固件 CRC 校验失败 → 停留在 Bootloader，不跳转损坏 APP
- 设置 `s_app_firmware_invalid` 标志，阻止 IDLE 超时自动跳转
- 循环打印等待恢复提示，支持重新发起升级

### A/B 分区设计

Flash 划分为 A/B 两个 APP 分区，Bootloader 根据校验结果选择跳转目标，升级失败时可回退至上一可用分区。

## 串口通信协议

自定义帧格式，大端序：

```
包头(0x66) | 命令(1B) | 长度(4B) | 分隔符(0x2F2F) | 载荷(NB) | CRC32(4B) | 包尾(0xBB)
```

- 信息帧 25 字节：携带固件大小、版本号、固件 CRC32
- 数据帧最大 1037 字节：携带固件分片
- 帧校验范围：命令 + 长度 + 分隔符 + 载荷

详见 `ESP32S3_STM32F407_引脚清单与通信协议.txt`。

## 目录结构

```
.
├── ESP32S3-OTA/              # ESP32-S3 固件 (ESP-IDF)
│   └── main/
│       ├── main.c            # 入口
│       ├── board/            # 板级驱动 (UART, WS2812)
│       └── iot/              # OneNET (MQTT/OTA/物模型/WiFi)
├── STM32-OTA/                # STM32F407 固件 (HAL + FreeRTOS)
│   └── testB3/
│       ├── Core/             # HAL 主程序
│       ├── Drivers/
│       │   ├── bootloader_drivers/  # Bootloader 核心 (协议解析/CRC/Flash)
│       │   ├── BSP/                 # 板级支持
│       │   └── STM32F4xx_HAL_Driver/
│       └── MDK-ARM/         # Keil 工程
└── ESP32S3_STM32F407_引脚清单与通信协议.txt
```

## 技术栈

- **ESP32-S3**: ESP-IDF v5.x，OneNET MQTT，HTTPS OTA，SPIFFS，WS2812 RMT 驱动
- **STM32F407**: HAL 库，FreeRTOS，LVGL，自定义 Bootloader，Flash A/B 分区
- **校验算法**: CRC32（流式累加，多项式 0xEDB88320）

## 硬件连接

| ESP32-S3 | STM32F407 | 说明 |
|----------|-----------|------|
| GPIO17 (TX) | RX (PA10/USART1) | 串口数据 |
| GPIO18 (RX) | TX (PA9/USART1) | 串口数据 |
| GND | GND | 共地 |

## 构建与烧录

### ESP32-S3

```bash
cd ESP32S3-OTA
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### STM32F407

使用 Keil MDK 打开 `STM32-OTA/testB3/MDK-ARM/` 下的工程，编译后通过 ST-Link 烧录。

## 版本

- `v1.0.8-crc-e2e` — 实现固件端到端 CRC32 校验，修复帧 CRC 语义错位 bug

## License

MIT
