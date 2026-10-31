# Python 串口 OTA 测试

用 PC 临时代替 ESP32-S3 作为串口主站，单独验证 STM32 Bootloader 的接收、校验、回滚和重新升级。测试固件来自实际 APP 工程，传输内容和 CRC 与正式协议一致。

目前推荐入口为 `ota_upgrade_fwcrc_capture.py`；实际断电使用 `ota_powerloss_capture.py`。实机结果见 [2026-10-31 验证记录](../DOC/verification/2026-10-31-uart-python-ota.md)。

## 准备

1. 使用 Python 3.10 或更新版本，在本目录执行 `python -m pip install -r requirements.txt`。本次离线检查使用 Python 3.11、pyserial 3.5。
2. 编译并烧录本仓库 Bootloader，确认运行中的 APP 可查询版本。APP BIN 不随源码提交，需要自行用 Keil 生成。
3. `ota_pack.py` 的 `FW_PATH` 默认指向仓库内正式 APP 的 `MDK-ARM/LVGL_RTOS_DOME5/LVGL_RTOS_DOME5.bin`；核对文件、大小、CRC 和声明版本。
4. 默认协议口 `COM6` → STM32 USART2，115200/8N1；调试口 `COM26` → USART1。关闭占用端口的串口助手，断开 ESP32 与 USART2 的收发线，保持单主站。
5. 更换电脑时按实际端口调整相关模块的 `PORT`；调试口可使用 `--debug-port`。当前入口没有统一的协议口命令行参数。

以下命令从本目录运行，假设运行中的 APP 和候选均为 `2.0.8`。版本由参数声明，不从 BIN 自动解析；`ota_pack.py` 的旧默认值不能当作当前实际版本。

## 实机命令

查询版本，不开始升级：

```powershell
python -u .\ota_query.py
```

正常升级：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --normal-upgrade --debug-port COM26 --version 2.0.8 --expect-current-version 2.0.8
```

针对从零计数的 `seq=10` 注入两段拆分（8+1029 字节、间隔 50 ms）：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --normal-upgrade --debug-port COM26 --version 2.0.8 --expect-current-version 2.0.8 --split-data-seq 10
```

多段拆分，累计切分点对应 1、1、6、512、512、4、1 字节：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --normal-upgrade --debug-port COM26 --version 2.0.8 --expect-current-version 2.0.8 --split-data-seq 10 --split-data-cuts "1,2,8,520,1032,1036" --split-gap-ms 50
```

一次提交最后两帧，检查两次 ACK。末包按实际长度发送，合计长度须不超过当前 2048 字节 DMA 临时区；不满足条件时在打开协议口前拒绝执行：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --normal-upgrade --debug-port COM26 --version 2.0.8 --expect-current-version 2.0.8 --glue-last-data
```

整体 CRC 拒收回滚：只把整体 CRC 声明改错，信息帧及各数据包 CRC 仍正确，要求结束时收到 `NACK 0x05`：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --debug-port COM26 --version 2.0.8 --expect-current-version 2.0.8
```

主动 ABORT，确认前 10 包后立即中止：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --debug-port COM26 --abort-after-packets 10 --version 2.0.8 --expect-current-version 2.0.8
```

数据阶段超时，确认前 10 包后静默 16 秒，检查恢复通知及 ABORT 收尾：

```powershell
python -u .\ota_upgrade_fwcrc_capture.py --debug-port COM26 --timeout-recovery --version 2.0.8 --expect-current-version 2.0.8
```

这些模式互斥。主采集入口未加 `--normal-upgrade` 且未选专用模式时，默认是整体 CRC 故障测试。恢复类测试完成后，保持设备运行，直接执行正常升级，检查新会话是否成功。

## 实际断电

```powershell
python -u .\ota_powerloss_capture.py --debug-port COM26 --version 2.0.8 --expect-current-version 2.0.8
```

此入口针对整板 USB 拔出后板载调试串口消失的接法：

1. 先确认 USB 拔出能使整板断电，其他接口没有继续供电，并准备原有烧录恢复工具。
2. 确认半份固件后，看到提示，在 8 秒内拔 USB，确认断电后输入 `OFF`。
3. 按住 RESET 重新插 USB，保持按住；端口枚举完成后输入 `READY`。
4. 等脚本重新打开调试和协议口，再按提示松开 RESET。启动恢复期间只接收并确认 APP 业务帧，不发送结束命令或 ABORT。

脚本检查“检测到未完成升级”的新启动日志、恢复 CRC 与旧 APP 版本。USB 消失是枚举证据，整板实际断电由操作者确认；按住 RESET 用于保留启动日志，不能代替真实断电。

## 输出与判定

每轮生成 `runs/<模式和时间>/`，原始记录默认不进入 Git：

| 文件/字段 | 含义 |
|---|---|
| `protocol.jsonl` | 收发字节块、时间、拆分/粘连等操作；一次读写不必然等于一帧或一次 DMA 回调。 |
| `stm32.bin` / `stm32.log` | 调试原始字节及 UTF-8 文本。 |
| `result.json` | 模式参数、协议结论、收尾、CRC 和阶段计时。 |
| `protocol_pass` | 该用例的协议断言是否完成，故障注入得到预期 NACK 也可通过。 |
| `cleanup.success` | ABORT ACK 和旧 APP 版本恢复是否确认；不会覆盖原始测试失败。 |
| `recovery_crc.match` | `true` 为备份/恢复 CRC 一致，`false` 为不一致，`null` 为证据不足或本轮未发生恢复。 |
| `test_pass` | 人工断电入口的整体判定，另含人工确认和启动恢复证据。 |

正常升级没有恢复 CRC，`match: null` 属于预期。最后结束命令已经发出时，脚本不自动 ABORT；异常收尾有等待上限，原始错误会保留。

计时终点是主机确认 APP 版本，包含 APP 初始化和主机等待；数据速率按有效 BIN 字节计算，不包含帧头、CRC、ACK 或填充。不把回滚耗时与正常升级混算，也不将 Python 串口速率当作云端完整 OTA 速率。

## 入口分工和离线检查

| 模块 | 用途 |
|---|---|
| `ota_pack.py`、`ota_query.py` | 组包和 APP 版本查询。 |
| `ota_upgrade.py`、`ota_upgrade_fwcrc_min.py`、`ota_upgrade_timeout.py` | 当前正常、整体 CRC、超时流程及有界收尾。 |
| `ota_upgrade_fwcrc_trace.py`、`ota_upgrade_fwcrc_capture.py`、`ota_timing.py` | 字节记录、统一实机采集入口和计时。 |
| `ota_powerloss_capture.py` | 人工断电及重新上电采集。 |
| `ota_upgrade_crc.py`、`ota_upgrade_dup.py`、`ota_upgrade_seq.py` | 早期单包 CRC、重复、错序用例，保留用于复现历史验证。 |
| `ota_upgrade_fwcrc.py`、`test_ota_serial.py` | 早期完整故障流程及其离线串口检查，当前实机推荐上方统一入口。 |
| `test_powerloss_offline.py` | 模拟端口消失、重连、缺版本/CRC/启动证据等分支。 |

```powershell
python -m unittest test_ota_serial test_powerloss_offline -v
```

以上测试使用模拟串口和虚拟时间，不打开真实设备；当前共 23 个测试方法。它们不覆盖真实 Flash 掉电行为，也不能替代更新后的 Bootloader 实机记录。
