# 项目进度交接文档（临时）

> 本文件为项目进度交接文档，供跨会话/跨 AI 工具对接使用。项目完成后会替换为正式 README。
> 原版 README（面向面试展示）已保存在 git 提交 028e37e 中，随时可用 `git show 028e37e:README.md` 找回。

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

## 五、后续路线图（未开始，按优先级）

1. **W25Q128 A/B 分区回滚**（下一个大目标）
   - 现状：A 区 APP 被擦写后原版本即丢失，升级失败只能等重传，无旧版本可退
   - 目标：外部 Flash W25Q128 存 B 区备份，升级失败/固件损坏时 Bootloader 自动回滚到旧版本（如 testA2）
   - 与协议改造的衔接：不可恢复 NACK（0x04/0x05）触发回滚
2. 固件签名验证（P1，面试加分项）
3. 断点续传：STM32 存 `received_offset`，重连后从断点继续（P2，依赖状态查询能力）

## 六、已知问题 / 注意事项

- **当前工作区有未提交改动**：协议健壮性改造（NACK+ABORT+重试上限+死锁修复）+ STM32 端两处测试注入（见第四节），测试收尾后一起提交
- v1.0.6/1.0.7/1.0.8 三次升级的固件 CRC 均为 0x586863AB，推测 bin 内容未变（版本号仅来自 OneNET 任务元数据）——**建议验证真实版本变化场景**
- `OTA_DATA_FRAME_HEADER_LEN` 语义已修正为 8（真头部），TAIL=5（CRC+包尾），MAX_LEN=1037 不变，解析偏移本来就正确，无线上协议影响
- ESP32 端 Step 1 的 0x01 只对 A 区 APP 有效，Bootloader 不处理 0x01——重试场景由 `stm32_in_bootloader` 参数跳过 Step 1 规避
- **不要用 vTaskSuspend/vTaskDelete 冻结或删除可能持有 UART 驱动内部互斥锁的任务**（死锁教训，见第四节）
- OneNET 上报语义：HTTP 响应 `code:0 succ` 只代表"报文被接收"，业务成败由 step 值决定；1xx 只能下载阶段报、2xx 只能升级阶段报
- 用户调试工具：串口助手看 STM32（USART1? 实际用 huart2 发协议帧+U1_Printf 打印），ESP32 用 idf.py monitor
