# 项目进度交接文档（临时）

> 本文件为项目进度交接文档，供跨会话/跨 AI 工具对接使用。项目完成后会替换为正式 README。
> 原版 README（面向面试展示）已保存在 git 提交 028e37e 中，随时可用 `git show 028e37e:README.md` 找回。

## 0. 当前进度（2026-08-29，A区智能哨兵 APP 初版实机联调）

### 下一战略里程碑

1. 先处理风扇供电/接地对 MQ-2 ADC 的干扰，确认 PA1/AO 峰值、电源压降和地线压差；当前风扇转动会使 MQ-2 偶发 4095，停扇后突跳消失。
2. 用 USART3（PB10/PB11）接入 RS485 工业温湿度采集器，先完成 Modbus RTU CRC、超时、重试和旧值保留，替换间歇失败的 DHT11。
3. 采集稳定后，把当前温湿度超限固定 80% 升级为 PI 环境量闭环；PWM 继续使用已验证的 100 Hz，烟雾报警始终以 100% 最高优先级覆盖。

### 本轮已落地

1. A区 APP 已加入 MQ-2、BH1750、双风扇 PWM、配置持久化、报警引擎、LVGL 数据显示和 STM32↔ESP32 新业务帧；OneNET 扩展为 Alarm/Fan/LED/Lux/Smoke/hum/temp 共 7 个属性。
2. 当前报警规则为严格 `>` 比较并连续约 2 秒成立：温度或湿度超限时风扇 80%，烟雾超限时风扇 100% 并触发声光；UI 风扇条只显示最终输出。
3. 光照默认滞回由 50/200 lx 改为 5/20 lx，并对 EEPROM 旧默认值做一次性迁移；自动照明默认开启，控制状态与 PF10 实际状态分开显示。
4. 风扇 PE5/PE6 输出及 MOS 开关波形已验证。当前两线风扇在 10 kHz、80% 时只响不转，改成 100 Hz 后正常起转。
5. MQ-2 采用 20 分钟预热屏蔽，预热期间只采样显示、不参与报警；实测冷机约 15 分 46 秒后才从 4095 降到稳定区间。
6. BH1750 遮挡后可接近 0，基本 I2C 和感光响应正常；固定房间光源下长期上升、DHT11 间歇 `read FAIL/recovered` 仍未解决。

详细设计、实施状态和踩坑证据见 [A区APP功能扩展方案.md](A区APP功能扩展方案.md)、[实施计划](.trae/documents/update_app_extension_plan_v2.md) 与 [进度记录](A区APP功能扩展方案进度记录.txt)。本轮文档收尾未重新编译；实机结论来自 BOSS 已完成的烧录、串口日志和示波器测量。

## 0.1 OTA进度（2026-08-24，回滚、版本闭环、IWDG与ESP32任务调度首轮收尾）

### ESP32-S3连续多轮OTA Manager（V4独立重启同步已实机通过，任务绑核源码已收尾）

1. ESP32-S3端改为独立的常驻`ota_manager.c/.h`：首次MQTT连接、`$sys/{pid}/{device}/ota/inform`通知和5分钟空闲兜底检查统一唤醒同一个Manager任务，不再为每轮OTA创建并删除临时任务。
2. OTA检查触发改用FreeRTOS任务通知，`ulTaskNotifyTake(pdTRUE)`会合并Manager忙期间累计的重复请求；STM32版本改由长度为1的覆盖队列传递，Manager独占最新版本缓存，删除原共享版本结构体、临界区和版本事件位。
3. SPIFFS从“每轮OTA重复注册”改为系统启动时只挂载一次，修复第二轮调用因`ESP_ERR_INVALID_STATE`直接退出的问题；固件文件仍在每次下载时以`O_TRUNC`覆盖。
4. MQTT OTA通知按`topic_len/data_len/current_data_offset/total_data_len`重组，完整JSON校验并成功投递Manager后才回复`ota/inform_reply`；通知只作为唤醒信号，目标版本、任务ID、大小和MD5仍以HTTP `check`接口为准。
5. MQTT客户端保留单实例保护；物模型周期上报任务已移回`onenet_dm.c`并在初始化时只创建一次，MQTT回调不再创建业务任务。Wi-Fi成功获取IP后清零本轮重试计数，NVS初始化增加错误处理，认证token不再输出到日志。
6. 单轮OTA流程已收成“准备OneNET任务、下载并校验固件、UART传输并确认版本”三个阶段；任务ID、目标版本、大小、MD5和CRC32收进单轮`onenet_ota_job_t`，不再由多组全局变量跨函数共享。
7. OneNET JSON HTTP请求移除多余的HTTP EventGroup：同步`esp_http_client_perform()`返回后直接解析回调已填充的有界缓冲区，并拒绝超过1023字节的响应。Bootloader、固件传输协议和现有回滚状态机本轮均未改动。
8. 实机调试发现ESP32单独重启后会丢失版本缓存，而未重启的STM32已因旧ACK停止主动上报。现新增`66 09 BB`版本查询：每轮Manager检查要求取得查询后的新版本报告，快速查询3次、每次等待500ms，并保留总计30秒的有界等待窗口。
9. ESP32-S3应用任务完成静态绑核收尾：OTA Manager独占Core 1，其余UART接收、Wi-Fi/OneNET启动、物模型更新和周期属性上报任务统一固定到Core 0。只将四处`xTaskCreate()`替换为`xTaskCreatePinnedToCore(..., 0)`，未增加任务、变量、函数或同步对象。

### ESP32-S3应用任务核心与优先级（当前源码配置）

| 任务 | 核心 | 优先级 | 当前职责 |
|---|---:|---:|---|
| `onenet_ota_manager` | Core 1 | 8 | 常驻OTA状态机、OneNET检查、HTTP下载、UART固件传输和版本确认。优先级由用户从2调整为8。 |
| `uart_recive_buff` | Core 0 | 10 | 普通UART事件接收、业务帧/版本帧解析与ACK。OTA独占期间不与Manager争用协议数据。 |
| `vMQTT_ONENET_Task` | Core 0 | 5 | NVS、Wi-Fi和OneNET启动，之后阻塞等待Wi-Fi连接事件。 |
| `oneNET_update_task` | Core 0 | 5 | 阻塞等待普通物模型UART队列并更新本地状态。 |
| `property_report_task` | Core 0 | 5 | 每500ms检查连接并执行OneNET物模型周期上报。 |

> 本阶段收尾边界：`66 09 BB`版本查询已经完成双端编译、烧录和V4实机验证；上表最终优先级与绑核配置已进入源码，但本次调度修改尚未单独编译、烧录或实机回归。后续仍应验证“无需复位ESP32/STM32连续升级两次”、OTA忙时重复通知、MQTT断线重连单实例和通知分片，不得把静态绑核完成写成实机性能提升已验证。

### 已完成的实机证据

| 用例 | 结果 | 关键证据 |
|---|---|---|
| R0：旧基线迁移 | 通过 | `testA2` 的 A 区大小为 `487228` 字节，迁移 CRC32 为 `0x2FCAE3E7`，向量表有效并成功跳转 APP。 |
| R1：正常升级 | 通过 | 先完成 W25Q128 旧镜像备份，再擦除/写入新镜像；`13460` 字节固件的整体 CRC32 `0x8C1C3844` 一致并跳转新 APP。 |
| R2：整体 CRC 失败 | 通过 | STM32 回 `NACK 0x05`，收到 ESP32 ABORT 后从 W25Q128 恢复旧镜像，恢复 CRC32 为 `0x2FCAE3E7`。 |
| R3：传输中 ABORT | 通过 | ESP32 在 `0x04` 获得 ACK 后主动发送 `66 08 BB`；STM32 ACK 后回滚，旧 APP 可重新启动。 |
| R4：升级中断电 | 通过 | 已分别验证擦除阶段和数据传输阶段断开 STM32 电源；重启后从 W25Q128 恢复旧镜像，CRC32 为 `0x2FCAE3E7`。旧会话收到 `NACK 0x08` 后以 ABORT 收尾，最终跳转旧 APP。 |

### W25Q128备份损坏测试边界

- 用户决定跳过“主动破坏W25Q128备份数据”的实机故障注入，不再将其作为当前阶段阻塞项。
- 代码仍保留备份记录CRC、备份镜像CRC和回滚后A区CRC三层检查；校验失败时设计行为是停在Bootloader。
- 该分支只能标记为“保护逻辑已实现、实机故障注入未验证”，不得写成测试通过。

### APP版本上报闭环（V0/V1/V2/V3/V4已实机通过）

1. 正式APP从AT24C02 `0x2C–0x33`读取Bootloader成功提交的8字节版本字段，只接受`X.X.X`或带`v/V`前缀的三段数字格式。
2. APP通过现有USART2单发送者任务发送固定帧：`55 05 major minor patch 00 00 BB`。每轮沿用200ms ACK和最多4次发送，一轮失败后每5秒重新上报，不阻塞传感器业务。
3. ESP32S3收到版本帧后只在本地缓存版本和报告序号，再回复`55 7E 00 00 00 00 00 BB`；UART任务不执行HTTP请求。
4. 每轮Manager检查由ESP32发送`66 09 BB`主动查询当前版本。STM32在通讯任务中重新开放版本帧发送资格，解决ESP32单独重启后本地缓存丢失、STM32仍持有旧ACK的问题。
5. MQTT连接后的OTA任务只接受本轮查询之后的新版本报告，删除硬编码`1.0.7`；先将当前版本上报OneNET，再使用该版本检查OTA任务。
6. 固件串口传输成功不再立即上报`step 201`。ESP32等待新APP版本报告最多30秒，只有数值版本与OneNET目标版本一致时才直接上报`step 201`；该状态会由平台把设备版本更新为任务目标版本，不再提前`POST /version`重复结束任务。
7. 30秒内只收到旧版本/其他版本，或完全没有新版本报告时，上报`step 206`；若收到旧版本，会先把实际版本同步回OneNET。
8. OneNET版本和状态接口各有3次、间隔2秒的有界重试。纯云端通信失败不会伪造成STM32固件失败，也不会发送虚假的`step 201`。
9. OneNET官方错误码`20 / task finish`表示任务已经完成；仅当上报`step 201`时将其视为幂等成功并停止重试，其他状态码仍按原错误路径处理。

### EEPROM布局（当前）

- `0x00–0x2B`：`OTA_INFOCB.Firelen[11]`，其中`Firelen[0]`为当前镜像大小、`Firelen[1]`为CRC32。
- `0x2C–0x4B`：`OTA_INFOCB.OTA_VERSION[32]`；当前串口协议使用其中前8字节。
- `0x4C–0x6B`：W25Q128备份记录（魔数、格式、版本、大小、CRC、记录CRC）。
- `0xB4–0xB7`：`backup_valid`、升级状态、失败原因、元数据版本。
- `0xB8`：恢复通知，仅在格式版本`0x31`下解释。
- `0xC0–0xCC`：正式APP电阻触摸屏校准参数。旧地址`0x28–0x34`与OTA元数据重叠，已迁移；首次运行需要重新完成一次触摸校准。

### 版本闭环测试用例

1. V0：无升级任务启动，确认APP版本帧、ESP32 ACK、OneNET当前版本上报和“无任务正常退出”。
2. V1：旧版本升级到新版本，确认顺序为“串口传输成功 → 新APP启动并上报目标版本 → ESP32确认版本一致 → `step 201` → OneNET更新为目标版本”。
3. V2：制造CRC失败或ABORT回滚，确认旧APP重新上报旧版本，ESP32最终上报`step 206`，不得出现`step 201`。
4. V3：APP先启动、ESP32晚启动，确认APP按5秒周期重试并最终收到ACK。已实机通过。
5. V4：通过。STM32保持APP `1.0.16`运行且已收到旧ACK，只重启ESP32；ESP32在`WAIT_STM32_VERSION`发送`66 09 BB`，STM32无需复位即重新上报`55 05 01 00 10 00 00 BB`，ESP32缓存为`report=1`并回复成功ACK。随后当前版本同步OneNET成功，平台返回`12012/not exist`表示当前没有OTA任务，本轮正常结束。
6. V5：EEPROM版本为空或格式错误。当前决定不做主动破坏EEPROM的故障注入，不作为进入下一任务的阻塞项。
7. 回归`0x01–0x04`物模型帧、业务ACK、`66 09 BB`版本查询和`66 01 BB`升级通知。

### IWDG首轮加固（W0–W3已实机通过）

1. `testB3` Bootloader使用STM32F407软件启动IWDG，配置为`Prescaler=/64`、`Reload=2499`，按32kHz标称LSI计算约5秒。
2. STM32F407数据手册给出的LSI范围为17–47kHz，因此实际复位窗口约3.4–9.4秒；日志中的“5000 ms”只表示名义值，不是精确定时保证。
3. Bootloader启动时锁存并打印`RCC_FLAG_IWDGRST`，随后清除RCC复位标志。若复位发生在已登记的`UPGRADING/NEED_ROLLBACK`阶段，原有EEPROM状态会在下次启动触发回滚。
4. A区Flash由一次擦除8个扇区改为逐扇区擦除并在扇区之间喂狗；W25Q128忙等待、镜像CRC、备份、回滚和OTA状态机循环均已加入喂狗点。
5. 正式APP在`HAL_Init()`后立即刷新可能由Bootloader遗留的IWDG，随后用相同参数重新配置；现有LVGL主任务负责周期喂狗。若该任务或调度器长期卡死，IWDG将复位MCU。
6. 调试器暂停CPU时冻结IWDG，避免断点停留被当作故障；因此不能用普通断点停住代码来验证看门狗复位。
7. 本轮不改STM32 Option Bytes，按`IWDG_SW`软件启动模式实现；烧录前应在Keil/CubeProgrammer确认目标板没有被改成硬件自动启动IWDG。
8. 启动恢复后的旧会话收尾窗口由普通启动的5秒单独延长为10秒；收到并ACK `66 08 BB`后立即结束等待并跳转旧APP，避免迟到ABORT落到APP业务协议中。

### IWDG验证结果

1. W0：通过。只上电不升级，Bootloader启动IWDG并由APP接管，未出现误复位。
2. W1：通过。完整OTA期间W25Q128备份、逐扇区擦除、480包传输、整体CRC和APP跳转均未触发IWDG误复位。
3. W2：通过。A区擦除后停止喂狗，STM32由IWDG复位；Bootloader检测未完成升级并从W25Q128恢复旧镜像，恢复CRC32为`0x179DA707`，旧APP `1.0.14`重新启动并上报，ESP32最终向OneNET上报`step 206`成功。该次日志还暴露出ESP32的ABORT晚于原5秒恢复等待窗口，代码已增加10秒恢复专用等待；该收尾时序没有单独的新日志记录，不与W3正常OTA回归混写。
4. W3：通过。关闭W2故障注入并恢复正式代码后，用户已完成正常OTA回归，未反馈IWDG误复位、升级链路或APP版本上报闭环异常。本次仅记录用户确认，未补写未提供的版本号、CRC和包数。

> W2故障注入已关闭：当前`OTA_IWDG_RESET_TEST_ENABLED=0`，Bootloader保持正常OTA流程。

### 本轮验证边界

- Codex未执行Keil或ESP-IDF编译；编译、烧录与W0–W3实机测试由用户完成，结论按已提供日志和用户确认分别记录。
- W3没有提交新的双端完整日志，因此只记录“用户确认通过”，不虚构固件版本、CRC、包数或平台请求结果。
- IWDG加固轮次本身未新增状态查询帧、会话ID或断点续传；后续OTA Manager调试已新增`66 09 BB` APP版本查询，但仍未引入会话ID和断点续传。

## 0A. 历史交接记录（本节以下的旧状态结论已被第 0 节覆盖）

### 本轮结论：OTA 与双端 UART 业务闭环均已实机测试通过

- **OneNET OTA 链路**：已验证 OneNET 平台下发固件 → ESP32-S3 下载并经 UART 传输 → STM32 Bootloader 擦除、写入、CRC32 校验 → 跳转 A 区 APP 的完整链路。
- **业务数据链路**：已验证 STM32 APP 发送物模型帧、ESP32-S3 接收并放入 OneNET 数据队列、ESP32-S3 回复 ACK、STM32 APP 确认成功后再发送下一帧的闭环。
- **本轮边界**：尚未实现 AT24C02 升级状态、W25Q128 备份/回滚、APP 启动后主动上报版本号并由 OneNET 确认升级完成。这些属于下一阶段，不应与已验证 OTA 传输闭环混为一谈。

### 当前 UART 协议

| 场景 | 方向 | 帧格式 | 当前行为 |
|---|---|---|---|
| 物模型业务数据 | STM32 APP → ESP32-S3 | `55 | type | data[5] | BB`（8B） | STM32 一次只发送一帧，等待 ACK。 |
| 物模型 ACK | ESP32-S3 → STM32 APP | `55 | 7E | result | 00 | 00 | 00 | 00 | BB`（8B） | `00`：已进入 OneNET 队列；`01`：帧或类型错误；`02`：队列满。 |
| APP版本查询 | ESP32-S3 → STM32 APP | `66 09 BB`（3B） | 每轮Manager检查主动请求；不单独回`0x66` ACK，版本帧即响应。 |
| APP版本报告 | STM32 APP → ESP32-S3 | `55 05 major minor patch 00 00 BB`（8B） | 启动时主动上报；收到`66 09 BB`后允许重新上报。 |
| OTA 升级通知 | ESP32-S3 → STM32 APP | `66 01 BB`（3B） | APP 收到后准备进入 Bootloader。 |
| OTA 通知 ACK | STM32 APP → ESP32-S3 | `66 02 BB`（3B） | ACK 的 DMA 发送完成后，APP 才复位进入 Bootloader。 |
| OTA 固件传输 | ESP32-S3 → STM32 Bootloader | 现有 `0x66` 可变长固件协议 | 沿用已验证的握手、信息帧、数据帧、CRC32 与 ACK/NACK 流程。 |

### 当前 UART 实现要点

- STM32 APP：USART2 接收中断只写入自定义 64B 环形缓冲区并通知任务；通讯任务是 USART2 DMA 的唯一发送者。ISR/DMA 回调中不发送、延时、解析完整语义或复位。
- STM32 ACK 策略：ACK 超时为 200 ms；首次发送之外最多重发 3 次；总计 4 次仍失败则丢弃当前帧并处理下一帧。HAL 时基由 TIM7 的 `HAL_IncTick()` 维护，保证 200 ms 超时能正常生效。
- ESP32-S3：`UART_DATA` 事件按“字节块”而不是“完整协议帧”处理，逐字节状态机支持拆包与粘包；只有完整业务帧成功进入 OneNET 队列后才回复成功 ACK。
- 调试日志：双端保留业务帧 TX/RX、ACK TX/RX、OTA 通知 TX/RX、DMA 完成、重传、丢弃、解析错误等包级日志；日志均位于任务上下文，不在 UART/DMA ISR 中打印。

### 下一阶段

1. 按 `STM32-OTA/W25Q128回滚方案设计.md` 实现 **W25Q128 单备份回滚** 与 AT24C02 状态记录。
2. APP 从 AT24C02 读取当前版本号，启动后发送给 ESP32-S3；ESP32-S3 上报 OneNET。该上报只作为“云端确认记录”，不作为 STM32 是否回滚的判据。
3. 再补齐断电、备份损坏、升级中断、回滚成功等异常测试与测试记录。

## 一、项目概述

**ESP32-S3 + STM32F407 双 MCU OTA 升级系统**

```
OneNET 云平台 ──HTTPS/MQTT──> ESP32-S3(WiFi中转+固件下载) ──UART自定义协议──> STM32F407(Bootloader+APP)
```

- ESP32-S3：ESP-IDF v5.x，OneNET MQTT 物模型，HTTPS 下载固件到 SPIFFS，串口分发给 STM32
- STM32F407：HAL + FreeRTOS + LVGL（APP 业务为温湿度控制），自定义 Bootloader 做升级
- 目标：秋招嵌入式岗位的面试项目

**Git**：https://github.com/ChenFU-CN13/esp32-stm32-ab-ota.git
- 分支：main / feature/ota-recovery（开发在 feature 分支）
- 当前 tag：`v1.0.8-crc-e2e`，最新提交 `028e37e`（README）
- 开发环境：STM32 用 Keil MDK + ST-Link；ESP32 用 ESP-IDF（Trae 里的 ESP-IDF 插件）

## 二、已完成（v1.0.8-crc-e2e，已测试通过）

### 1. 端到端 CRC32 三层校验防线
| 层 | 对象 | 作用 |
|---|---|---|
| 传输层 | 每个数据帧 CRC32 | 检出 UART 位翻转 |
| 命令层 | 信息帧帧 CRC 自校验 | 擦 Flash 前拦截坏帧（防元数据错位/篡改） |
| 落盘层 | 整体固件 CRC32 | 写完 Flash 后兜底（检出 write_addr bug/截断/对齐填充错误） |

- 流式 CRC32：多项式 `0xEDB88320`，init/final 异或 `0xFFFFFFFF`
- ESP32 下载时在 `HTTP_EVENT_ON_DATA` 里逐块累加（与 MD5 同处），收尾得整体 CRC，随信息帧下发
- 修复过的核心 bug：原信息帧的帧 CRC32 被 STM32 误存为 `g_ota_ctx.firmware_crc32`（语义错位），通过信息帧扩到 25 字节、独立加固件 CRC32 字段解决

### 2. 防变砖策略（已实现）
- 整体 CRC 失败 → 停在 Bootloader 不跳转
- `s_app_firmware_invalid` 标志阻止 IDLE 超时自动跳转，循环打印等待恢复
- UPGRADE_FAILED 分支重置回 IDLE 等待重试（不跳损坏 APP）

### 3. 测试验证（三用例全通过）
- ①正常链路：计算 CRC = 期望 CRC = 0x586863AB，跳转 APP 成功
- ②人为 addr+1 制造校验失败：停在 Bootloader 循环打印
- ③篡改 `data[8]`：帧 CRC 自校验拦截（计算 0xAB294B37 ≠ 接收 0x9D303A52），不擦 Flash
- 测试注入代码已全部清理，**需确认烧录的是干净版本**

### 4. Git 工程化
- .gitignore 完善，编译产物已从跟踪中移除
- 已有 commit `b97183e`（CRC 端到端）+ `028e37e`（README）+ tag `v1.0.8-crc-e2e`，均已推送

## 三、通信协议现状（重要：对接前必读）

帧格式：`包头0x66 | 命令1B | ... | 包尾0xBB`，大端序。详细文档见项目根目录
`ESP32S3_STM32F407_引脚清单与通信协议.txt` 第七节。

### 命令码表（现行）
| cmd | 含义 | 方向 |
|---|---|---|
| 0x01 | UPGRADE_NOTIFY 升级通知（发给 A 区 APP，收到后重启进 Bootloader） | ESP32→STM32 |
| 0x02 | ACK 应答 | STM32→ESP32 |
| 0x03 | CONFIRM 握手确认 | ESP32→STM32 |
| 0x04 | START_TRANSFER 开始传输 | ESP32→STM32 |
| 0x05 | FIRMWARE_INFO 固件信息帧（25B） | ESP32→STM32 |
| 0x06 | FIRMWARE_DATA 固件数据帧（变长，max 1037B） | ESP32→STM32 |
| 0x07 | UPGRADE_END 升级结束（触发整体 CRC 校验+跳转） | ESP32→STM32 |
| 0x08 | ABORT 放弃传输，STM32 重置回 IDLE（≥WAIT_INFO 时置损坏标志防跳转） | ESP32→STM32 |
| 0x09 | VERSION_REQUEST 请求运行中的APP重新上报当前版本 | ESP32→STM32 APP |
| 0x0A | NACK 否定应答，帧 `66 0A err_code BB`（4B） | STM32→ESP32 |

### 信息帧 25 字节布局
```
66 | 05 | size(4) | 2F 2F | version(8) | fw_crc32(4) | frame_crc32(4) | BB
```
- frame_crc32 覆盖 packet[1..19] 共 19 字节
- fw_crc32 = 整个固件文件的 CRC32（ESP32 下载时流式算出）

### 数据帧布局
```
66 | 06 | packet_num(2) | data_len(4) | data(≤1024) | crc32(4) | BB
```
- 头 8B + 数据 1024B + 尾 5B = 最大 1037B
- 停等协议：发一帧等 ACK，超时 8s，最多重试 5 次

### STM32 状态机
```
IDLE → (0x01 重启进Bootloader后) 握手 → WAIT_INFO →(0x05) WAIT_DATA →(0x06×N) →(0x07) 整体CRC → 跳转APP
```
Flash 布局：Bootloader 0x08000000（4×16KB），A 区 APP 0x08010000（8 扇区 = 128KB）。

## 四、协议健壮性改造（NACK + ABORT + 重试上限）——**已完成，三用例实测全过，201/206 上报闭环**

### 背景问题（篡改测试暴露的死锁链）
STM32 对"帧 CRC 失败/状态不匹配/未知命令"**全部沉默不回包** → ESP32 只能超时重发 → 整体重试从 Step 1（0x01）重来，但 0x01 的接收方是已不在运行的 A 区 APP，Bootloader 命令分发里没有 0x01 的 case → 无人应答 → 死循环（`onenet_ota.c` 的 `for(;;)` 无重试上限，永不退出、不上报云平台）。

### 已实现（双端代码均已完成并烧录实测）
**新增 2 个命令**：
- `0x08` ABORT：ESP32→STM32，任意状态收到即 `OTA_Init()` 重置回 IDLE + 回 ACK。状态 ≥ WAIT_INFO 时 A 区已被擦写，置 `s_app_firmware_invalid=1` 防止超时跳转跑飞
- `0x0A` NACK：STM32→ESP32，帧格式 `66 0A err_code BB`（4B），err_code：
  - 0x01 帧CRC失败 / 0x02 状态不匹配 / 0x03 长度非法 / 0x04 Flash写入失败 / 0x05 固件整体CRC失败 / 0x06 未知命令 / 0x07 包序号异常

**ESP32 决策表**（已实现在 `send_packet_with_retry`）：
| NACK 错误码 | 性质 | ESP32 动作 |
|---|---|---|
| 0x01/0x03/0x07 | 可恢复（瞬时干扰） | 重发同一帧（走现有重试） |
| 0x02 | 时序错位 | 立即失败 → 发 ABORT 对齐 → 整体重试 |
| 0x04/0x05 | 不可恢复（硬件/固件问题） | 停止传输，上报 OneNET |

**配套改动**（全部已落地）：
1. STM32：`OTA_SendNACK(err_code)`；所有"沉默分支"补发 NACK；ABORT case；重复包幂等（`expected-1` 的重复包补 ACK，其他序号异常回 NACK 0x07）；**0x07 改为先整体 CRC 校验再回 ACK/NACK**（原时序 bug：先 ACK 后校验，NACK 0x05 发出时 ESP32 已退出无人听）
2. ESP32：`send_packet_with_retry` 加 NACK 识别分支；`send_abort()`（含 `g_last_nack_err` 保存-恢复防覆盖）；`serial_send_firmware` 加 `stm32_in_bootloader` 参数（重试时跳过 Step 1）；cleanup 仅在失败时发 ABORT（原无条件发导致成功后白等 40s）
3. onenet_ota.c：`for(;;)` 加 3 次重试上限，超限上报 OneNET **`step:206`（升级失败·未知异常）** 后删任务退出。注意：不是 108——OneNET 规则 1xx 只能下载阶段报、2xx 只能升级阶段报，串口传输失败属升级阶段。**成功路径还需补报 201**（待办）

### 改造过程中发现并修复的重大 bug：vTaskSuspend 驱动死锁
**现象**：用例 2 第二次重试时 ESP32 永久卡死在 `Step 2: Sending handshake confirm...`，无任何超时/重试日志，MQTT 正常（只死了 OTA 任务），STM32 等握手 5s 超时循环。
**根因**（IDF 5.x 驱动源码实证）：
1. `uart_read_bytes()` 拿 `rx_mux` 后整个读取期间持锁（含空 ringbuffer 时阻塞等数据 1000ms）
2. `uart_flush_input()` 内部 `xSemaphoreTake(rx_mux, portMAX_DELAY)` 无限等锁
3. 第一次传输期间接收任务被 `vTaskSuspend` 挂起，但 NACK/ABORT-ACK 的 UART_DATA 事件积压在 uart_queue（字节被 OTA 任务读走，事件还在）
4. `vTaskResume` 后接收任务消化过期事件 → 持 rx_mux 阻塞在 `uart_read_bytes(1000ms)`
5. OTA 任务第二次 `serial_send_firmware` → `vTaskSuspend` 把**持锁任务冻结** → 锁永不释放
6. OTA 任务 `uart_flush_input()` 无限等锁 → 永久卡死，握手帧根本没发出
**核心教训**：`vTaskSuspend` 无法控制任务被冻结在代码哪一行，冻在持锁区间就是死锁。FreeRTOS 官方明确警告不得挂起持有互斥锁的任务。
**修复**（已实测验证）：废弃 suspend/resume，改 `volatile bool g_ota_uart_busy` 标志门控——
- 接收任务 OTA 期间对 UART_DATA/BUFFER_FULL/FIFO_OVF 事件只消费不碰 ringbuffer（continue）
- `serial_send_firmware` 开头置 true；cleanup 里 `send_abort` 之后置 false + `uart_flush_input` + `xQueueReset(uart_queue)`（清过期事件，防接收任务误发 0xFF 给 STM32）
- 旧代码以注释块保留在 myuart.c 供对比

### 测试进度（三用例全过，2026-08-21 收官）
- **用例 1（正常升级回归，注入删除后复测）：通过**（23:04 实测）。全程 ACK，零 NACK/ABORT/retry，476/476 包 → CRC 校验成功（计算=期望=0x586863AB，证明注入已删干净）→ 跳转 A 区 APP 成功
- **用例 2（篡改 data[8] 持续帧 CRC 失败）：通过**。NACK 40ms 秒回 ×5 → ABORT 对齐 → 3 次重试上限 → 上报 `{"step":206}` OneNET 回 `code:0 succ` → STM32 停恢复循环（防变砖生效）。死锁修复实证：重试的 Step 2 从"永久卡死"变为 10ms 内回 ACK
- **用例 3（固件整体 CRC 失败）：通过**（2026-08-21 22:59 实测）。数据 476/476 包全传完 → 0x07 → STM32 整体 CRC 失败（计算 0xC813C421 ≠ 期望 0x586863AB）→ NACK 0x05 → **ESP32 立即停止不重试**（`Unrecoverable NACK=0x05, abort immediately`，靠 onenet_ota.c 外层新加的 0x04/0x05 短路判断）→ ABORT 对齐 → 上报 `{"step":206}` OneNET 回 `code:0 succ` → STM32 停在 Bootloader 恢复循环（防变砖生效）。中间曾发现 bug：0x05 收到后仍走 3 次重试白烧 487KB——已修（决策表落地到代码）

### 测试注入代码（已全部删除 ✅）
1. `TaskTest.c` → `OTA_ParseFirmwareInfo` 入口：`data[8] ^= 0xFF;`（用例2）— 已删
2. `TaskTest.c` → `OTA_VerifyFirmwareCRC`：`addr += 1;`（用例3）— 已删
回归用例 1 已通过（CRC 计算值与期望一致证明删干净），当前固件为干净版本。

### 剩余待办（协议改造收尾）
1. ~~用例 3~~ ✅ ~~删注入+回归用例 1~~ ✅ ~~成功路径补报 201~~ ✅（23:07 实测 `{"step":201}` → `code:0 succ`，OTA 云端闭环）
2. 可选优化：STM32 `OTA_WaitForHandshake` 对 0x01 也回 ACK（STM32 已在 bootloader 时 ESP32 Step 1 白等 5×8s=40s）
3. 可选修文案：`retry 3/3, will retry...` 最后一次仍打印"will retry"（打印先于循环条件判断，纯外观）；"开始擦除A区Flash..."重复打印两次（纯外观）
4. Git 提交本次协议改造（含死锁修复）

## 五、后续路线图（当前，按优先级）

1. **形成当前稳定基线**
   - 整理当前未提交改动，删除或隔离临时测试代码与生成文件，统一第0节和历史章节的结论后建立Git检查点。
2. **固件签名验证与防降级（下一大目标，P1）**
   - 当前ESP32的MD5和STM32的CRC32用于发现传输损坏，尚不能证明固件来自可信发布者。
   - 规划带版本、大小、摘要和数字签名的固件清单，由Bootloader内置公钥验证，并增加非法降级/旧包重放保护。
3. **OTA协议V2与断点续传（P2）**
   - 增加状态查询、会话ID、已接收偏移和双端重启后的状态同步；在安全边界明确后再实现断点续传。
4. **发布与测试工程化（P3）**
   - 增加固件打包、版本生成、摘要/签名生成、日志分级、密钥隔离和可重复的故障测试记录。
5. **ESP32S3自身OTA（可选扩展）**
   - 在STM32 OTA链路稳定后，再评估双MCU分别升级和版本兼容策略；当前不作为阻塞项。

## 六、已知问题 / 注意事项

- **当前工作区仍有大量未提交改动**：包括协议、回滚、版本上报、IWDG、正式APP和ESP32侧改造；开始签名功能前应先按目标工程整理并形成稳定Git基线。
- W2临时故障注入已关闭；以后新增故障注入必须使用明确开关，并在测试结束后恢复正式配置。
- 主动破坏W25Q128备份和EEPROM版本区的故障注入已决定跳过，只能标记为“保护逻辑存在、未实机注入验证”。
- `OTA_DATA_FRAME_HEADER_LEN` 语义已修正为 8（真头部），TAIL=5（CRC+包尾），MAX_LEN=1037 不变，解析偏移本来就正确，无线上协议影响
- ESP32 端 Step 1 的 0x01 只对 A 区 APP 有效，Bootloader 不处理 0x01——重试场景由 `stm32_in_bootloader` 参数跳过 Step 1 规避
- **不要用 vTaskSuspend/vTaskDelete 冻结或删除可能持有 UART 驱动内部互斥锁的任务**（死锁教训，见第四节）
- OneNET 上报语义：HTTP 响应 `code:0 succ` 只代表"报文被接收"，业务成败由 step 值决定；1xx 只能下载阶段报、2xx 只能升级阶段报
- 用户调试工具：串口助手看 STM32（USART1? 实际用 huart2 发协议帧+U1_Printf 打印），ESP32 用 idf.py monitor
