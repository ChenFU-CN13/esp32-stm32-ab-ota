# 项目文档导航

`DOC` 只保存适合公开、能解释设计或支撑验证结论的资料。根目录 `README.md` 是项目展示入口，本目录负责承载细节和历史过程。

## 1. 架构与协议

| 文档 | 内容 |
|---|---|
| [A区 APP 功能扩展方案](architecture/A区APP功能扩展方案.md) | APP 分层、FreeRTOS 任务、传感器、报警、风扇和 LVGL 设计。 |
| [ESP32-S3 与 STM32F407 引脚及通信协议](architecture/ESP32S3_STM32F407_引脚清单与通信协议.txt) | 双端接线、UART 帧、命令码、Flash 布局和代码依据。 |

## 2. 调试与故障复盘

| 文档/目录 | 内容 |
|---|---|
| [故障复盘](debugging/README.md) | 固件碎片化和 FreeRTOS UART 死锁两次代表性问题。 |
| [IAP Flash错误/修复代码对比](debugging/code-comparison/iap_flash_write_compare.c) | 恢复早期分页写错误代码，并用 `#if 0` 与修复版本同文件对比。 |
| `debugging/flash-fragmentation/` | ST-LINK Utility 固件对比图。 |
| `debugging/ota-transfer/` | 固件 UART 传输过程截图。 |
| `debugging/logic-analyzer/` | 串口与 Modbus 逻辑分析仪截图。 |

## 3. 实机验证

| 文档/目录 | 内容 |
|---|---|
| [验证状态与证据边界](verification/README.md) | 已验证、用户确认、尚未验证三类结论。 |
| [串口组帧与 Python OTA 验证](verification/2026-10-31-uart-python-ota.md) | 拆帧/粘帧修复、异常恢复后再次升级、实际断电及 2.0.8 双端正常 OTA。 |
| [Python 测试运行说明](../Python_test/README.md) | 接线、依赖、故障注入命令、采集字段和离线检查。 |
| [累计式环形缓冲区更新](verification/2026-10-29-累计式环形缓冲区更新.md) | APP/Bootloader 软件环改动、版本验证边界、ESP32 日志调整及 Python 测试接入计划。 |
| OneNET 原始截图 | 含账号、产品和设备标识，暂放本地忽略目录，裁剪后再选择性公开。 |

## 4. 开发记录

| 文档 | 定位 |
|---|---|
| [STM32 A区 APP 开发与实机验证记录](development-notes/STM32_A区APP开发与实机验证记录.md) | APP 侧当前状态、实机问题和后续建议。 |
| [APP 功能扩展实施计划](development-notes/APP功能扩展实施计划.md) | APP 扩展任务拆分和实施过程。 |

## 5. 图片素材

- `images/hardware/`：整机、模块和风扇驱动资料。
- `images/ui/`：LVGL 首页和设置页。

## 公开边界

- `local_materials/` 和 `.trae/` 只保存本地规划、打磨记录，不进入 Git。
- 本地芯片手册、AI 聊天记录、备份工程、编译产物和 OTA 视频不进入源码仓库。
- 公开文档只陈述源码可核对、日志已保存或用户明确确认的结果；没有完整证据的内容会标明边界。
