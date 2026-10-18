# A区 APP 功能扩展 — 实施计划（Codex 执行指南）

> 本文档由 `update_app_extension_plan_v2.md` **重构**而来，定位从「文档修订计划」升级为「**代码实施指南**」。
> 与 `A区APP功能扩展方案.md` 配合使用：
>
> - **方案MD** 定义「做什么、为什么、设计细节」（引脚、阈值、决策表、API 原型）。
> - **本文档** 定义「**按什么顺序写、每个模块先写什么再写什么、改哪些文件、如何验证**」，是方案MD 的**真正落实者**。
>
> Codex 拿到本文档时，请**从上到下顺序执行**「第 4 节 分步实施」，每步做完再进入下一步，不要跳步。

> **2026-10-17 执行状态**：初版S1~S5及“RS485 Modbus替换DHT11”已完成，正式版v2.0.0双端OTA回归通过。现阶段转入发布素材、凭据隔离和Git稳定基线整理。本文第4节仍保留初版历史实施顺序，不能再把其中的DHT11描述当作当前源码状态。

> ⚠️ **首要注意事项（必读）：禁止过度设计！**
> 严格按本文档 + 方案MD 的**初版范围**实现，用**最简单直接**的方式写完初版：不提前实现延后项（PI/RS485/0x8X 下发/复位报警/历史记录）、不做多余抽象与泛化、不顺手重构无关旧代码。
> **目标只有一个：先让初版功能能跑、能测、能回归。** 任何「顺带优化/扩展/重构」的念头，先记到笔记里，等初版验证通过后再一起讨论。

***

## 0. 使用说明与代码范围

### 0.1 两份文档的分工（Codex 必读）

| 内容                                           | 以谁为准                     |
| -------------------------------------------- | ------------------------ |
| 硬件引脚、阈值默认值、联动决策表、各驱动 API 原型                  | **方案MD**（第二、四、五、六章）      |
| 协议号约定（0x06/0x07/0x08/0x0A）、合并双风扇单指令          | **本文档**（0.3 关键决策 + 步骤 6） |
| 文件实际路径、工程注册、编码顺序、验证方法                        | **本文档**                  |
| 初版延后项及当前完成状态（RS485已完成，PI等仍延后） | **本文档**（0.2 + 第 6 节）     |

> ⚠️ **路径勘误**：方案MD 附录 A 中的目录名 `A区智能温湿度控制系统v2.0` 已改名，**实际工程目录为**：
> `STM32-OTA/A区智能哨兵环境监测系统/`。下文所有路径均以实际目录为准。

### 0.2 初版范围（本次实施只做这些）

| 项                       | 做 / 不做      | 说明                                             |
| ----------------------- | ----------- | ---------------------------------------------- |
| 温度、湿度采集上报               | ✅ 已升级 | SHT30Task + USART3 Modbus；本地x10定点数，UART2 type 0x01/0x02仍发整数 |
| **光照监测（BH1750）**        | ✅ 做         | 新驱动 + LightTask + type 0x07 上报 + UI            |
| **烟雾监测（MQ-2）**          | ✅ 做         | 新驱动 + SmokeTask + type 0x06 上报 + UI + 报警联动     |
| **双风扇 PWM 调速（PE5/PE6）** | ✅ 做         | 替换原 FAN.h GPIO 方式；type 0x08 单指令（双风扇同步）         |
| **阈值本地配置（AT24C02）**     | ✅ 做         | config\_manager + 设置屏 +/- 与保存按钮                |
| **IOT 心跳连接状态（0x0A）**    | ✅ 做         | ESP32 5s 发心跳 → STM32 解析 → UI `screen_iot_flag` |
| 三级报警引擎 + 声光 + 风扇联动      | ✅ 做         | 烟雾(快闪+滴答+100%) / 温湿度(80%) / 光照(绿灯)             |
| PI 环境量闭环               | ❌ 初版不做      | 温湿度超限固定 80%；待 RS485 采集稳定后单独实现     |
| RS485 Modbus 替换 DHT11   | ✅ 已完成正式接入 | PB10/PB11外接自动方向TTL-RS485模块；正常通讯和UI显示已由BOSS确认 |
| OneNET 云平台下发阈值 0x8X     | ❌ 延后        | 设置只走本地 LVGL                                    |
| 手动复位报警 + 120s 冷却期       | ❌ 延后        | 见第 6 节，初版一律不写                                  |
| 报警状态独立页面                | ❌ 不做        | 主屏已有红灯/蜂鸣器/风扇控件体现                              |

### 0.3 相对方案MD 的关键决策（本文档锁定，Codex 直接照做）

1. **双风扇合并为单指令 0x08**：`0x08 风扇状态 data[0]=0x00停止/0x01~0x64占空比%`，双风扇**同步**输出相同占空比；**删除原 0x09 风扇2 指令**。硬件仍接 PE5+PE6 双路 PWM，软件层同步写。
2. **心跳帧 type 0x0A（业务 0x55 帧）**：`55 0A status 00 00 00 00 BB`，`status=0x01在线/0x00离线`，ESP32→STM32，5s 周期；STM32 15s（3 倍周期）未收到判离线。与 OTA 协议（0x66 帧）中的 0x0A NACK 因帧头不同**不冲突**。
3. **风扇控制走 fan\_controller 单点调度**（方案MD 4.6），**不新增 fanTask 队列**：AlarmEngine 写 0/80/100% 目标，`FANCTRL_Run()` 统一写两路 PWM；UI bar 只显示最终值，不接受手动输入。
4. **温湿度超限风扇占空比固定 80%**：烟雾 100%，正常 0%；PI 留到 RS485 升级阶段再做，不预埋多余假逻辑。
5. **UI 新值走「全局结构体 + 500ms 轮询」**（方案MD 7.2 策略）：smoke/lux/fan/iot 不新增队列，StartDefaultTask 每 500ms 读全局刷新；SHT30温湿度继续使用长度1的覆盖队列，并保留周期快照刷新。
6. **周期上报合并进 AlarmEngineTask**（每 25 个 200ms 周期 = 5s）：一次拼 烟雾/光照/风扇 三帧发 UART2，不另开 PropertyReportTask。
7. **去抖用周期计数**（200ms 一个 tick，2s = 10 tick），**声光闪烁用 HAL\_GetTick() 相位计算**，不在任务里 osDelay 阻塞。
8. **取消 CRC32 校验（本次简化）**：config\_manager **不实现/不校验 CRC32**，仅以 `magic` 魔数判断 EEPROM 数据有效性（简化工作量）。CRC 字段 `crc32`/`reserved2` 从 `app_config_t` 中删除，EEPROM 写入长度相应缩减（见步骤 3）。

### 0.4 实机联调后的锁定结论

1. 当前 PWM 为 **100 Hz**：TIM9 计数频率 10 kHz、ARR=99。原 10 kHz、80% 输出波形正常但风扇只响不转；改 100 Hz 后正常起转。
2. 当前光照默认滞回为 **5/20 lx**，并把 EEPROM 旧默认 50/200 lx 一次性迁移为 5/20 lx。
3. 自动照明默认开启；LVGL 分开显示“控制开启/关闭”和照明灯“已开启/关闭”。
4. MQ-2 当前预热门控为 20 分钟，期间采样显示但不参与报警。实测约 15 分 46 秒才从 4095 降到稳定区间。
5. 风扇工作时 MQ-2 偶发 4095，停扇后突跳消失；BH1750 遮挡后可接近 0，但固定光源下长期变化原因未定。本轮只记录，不修改这些小 BUG。

***

## 1. 初版历史盘点（2026-08开始编码前的快照）

> 本章用于保留S1~S5最初实施依据，其中的DHT11Task、旧队列和“未完成项”不是2026-10-17当前源码状态。当前状态以本文0.2、0.4、第6节和根目录进度记录为准。

### 1.1 已完成、本次**不要动**的部分

| 位置                                        | 现状                                                                                                                                                                                                                                                                                                                          |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `LVGL/myGUI/generated/gui_guider.h`       | UI 结构体已含：`screen_bar_fan`（bar 0\~1000）、`screen_fan_num`、`screen_smoke_num`、`screen_sunlight_num`、`screen_iot_flag`（label，初值"NC"）、`screen_sw_light`、`screen_sw_buzzer`、`screen_sw_Alarm_light`；设置屏 `screen_setting_add_1~4/minus_1~4`、`num_temp/num_hum/num_sunlight/num_smoke`、`btn_save/btn_reset/btn_default/btn_Version` |
| `Core/Src/APP_Tasks.c`                    | 已有回调 `screen_sw_light_event_cb`、`screen_sw_buzzer_event_cb`、`screen_bar_fan_event_cb`（拖 bar → `g_fan_pwm` + 刷 `screen_fan_num`）、`screen_sw_Alarm_light_event_cb`；全局 `g_fan_pwm`、`g_alarm_light_sw`；任务 DHT11Task/lightTask/buzzerTask；队列 xUI\_Datas\_Queue/xLEDQueue/xbuzzerQueue/xUART2\_TxQueue；互斥锁 xDHT11Mutex            |
| `Core/Src/freertos.c`                     | `StartDefaultTask` 已注册上述 4 个回调；LVGL 任务循环 + 看门狗喂狗                                                                                                                                                                                                                                                                            |
| `Core/Src/uart2_comm.c`                   | 完整可靠传输：业务帧 0x55 8 字节、ACK 0x7E、OTA 0x66、版本上报 0x05（读 AT24C02\@0x2C）、200ms 超时重发 3 次                                                                                                                                                                                                                                            |
| `Core/Inc/stm32f4xx_hal_conf.h`           | ✅ `HAL_ADC_MODULE_ENABLED` / `HAL_TIM_MODULE_ENABLED` 已开                                                                                                                                                                                                                                                                    |
| Keil 工程 `MDK-ARM/LVGL_RTOS_DOME5.uvprojx` | ✅ `stm32f4xx_hal_tim.c` / `stm32f4xx_hal_tim_ex.c` 已注册（`Drivers/STM32F4xx_HAL_Driver` 组）                                                                                                                                                                                                                                    |

### 1.2 未完成、本次要写的部分

| 缺失项                                     | 说明                                                   |
| --------------------------------------- | ---------------------------------------------------- |
| `fan_pwm.c/h`                           | TIM9 CH1/CH2 PWM（PE5/PE6），替换 `FAN.h` 的 GPIO 方式       |
| `mq2.c/h`                               | ADC1\_IN1（PA1）采样                                     |
| `bh1750.c/h`                            | 软件 I2C（PB6/PB7）读 lux                               |
| `Middlewares/Services/`（新建目录）           | `config_manager` / `alarm_engine` / `fan_controller` |
| SmokeTask / LightTask / AlarmEngineTask | 新增 3 个 FreeRTOS 任务                                   |
| UART2 物模型扩展                             | 0x06 烟雾 / 0x07 光照 / 0x08 风扇 / 0x0A 心跳解析              |
| UI 联动                                   | smoke/lux/fan/iot 数值刷新 + 设置屏阈值 + 保存                  |
| Keil 工程注册                               | `stm32f4xx_hal_adc.c` 未注册（**必须加**）；新 .c 需注册          |

### 1.3 关键文件索引

| 文件                               | 路径                                                             | <br />                         |
| -------------------------------- | -------------------------------------------------------------- | :----------------------------- |
| 任务/数据结构                          | `Core/Inc/APP_Tasks.h`、`Core/Src/APP_Tasks.c`                  | <br />                         |
| UI 刷新/事件                         | `Core/Src/freertos.c`                                          | <br />                         |
| UART2 协议                         | `Core/Inc/uart2_comm.h`、`Core/Src/uart2_comm.c`                | <br />                         |
| 现有模块驱动（参考模板）                     | \`Drivers/Module\_driver/inc                                   | src/{FAN,DTH11,buzzer}.{h,c}\` |
| EEPROM 驱动                        | `Drivers/BSP/24CXX/24cxx.h`（`at24cxx_init/read/write`）         | <br />                         |
| 软件 I2C（EEPROM 用，PB8/PB9，**勿复用**） | `Drivers/BSP/IIC/myiic.h`                                      | <br />                         |
| Keil 工程                          | `MDK-ARM/LVGL_RTOS_DOME5.uvprojx`（`Module_driver` 组在 1875 行附近） | <br />                         |
| 方案设计                             | `A区APP功能扩展方案.md`                                               | <br />                         |

***

## 2. 总体实施顺序（依赖总览）

> 依赖原则：**驱动 → 数据模型 → 配置 → 采集任务 → 报警引擎 → 协议 → UI → 联调**。
> 每一级只依赖前几级，不跨级跳写。**每步完成后先编译 + 自测再进入下一步。**

| 步骤    | 模块          | 产出                                                | 依赖      | 验收（做完此步）                           |
| ----- | ----------- | ------------------------------------------------- | ------- | ---------------------------------- |
| **1** | BSP 驱动层     | fan\_pwm / mq2 / bh1750                           | 无（硬件）   | 3 个驱动单独串口打印验证通过                    |
| **2** | 数据模型与共享层    | sensor\_Data\_t、alarm\_input\_t、全局变量              | 无（纯类型）  | 编译通过，无回归                           |
| **3** | 配置管理        | config\_manager（magic 魔数校验，无 CRC32）               | 24cxx   | 改值→断电→重启值不变                        |
| **4** | 采集任务        | SmokeTask / LightTask                             | 步骤1+2   | 串口打印 ADC/lux 正常，任务稳定               |
| **5** | 报警引擎+风扇控制   | alarm\_engine / fan\_controller / AlarmEngineTask | 步骤1\~4  | 模拟触发→声光/风扇按决策表动作                   |
| **6** | UART2 物模型扩展 | 0x06/0x07/0x08 上报 + 0x0A 心跳解析                     | 步骤2     | ESP32 monitor 见 3 种新帧；心跳灯变化        |
| **7** | UI 联动       | 数值刷新 + 设置屏阈值 + IOT\_FLAG                          | 步骤3+5+6 | 触摸改阈值保存断电不丢；首页全量刷新                 |
| **8** | ESP32 端心跳发送 | ESP32 工程加 5s 心跳任务                                 | —       | STM32 的 `screen_iot_flag` 显示 ON/NC |
| **9** | 联调收尾        | Keil 注册、编译、烧录、版本号                                 | 全部      | 0 错误、新 .bin 生成、全链路测试用例通过           |

***

## 3. 全局约定（每步都遵守）

### 3.1 目录与分层（沿用现有「伪操作系统分层」风格）

```
A区智能哨兵环境监测系统/
├── Drivers/Module_driver/
│   ├── inc/  fan_pwm.h  mq2.h  bh1750.h     ← 步骤1 新建
│   └── src/  fan_pwm.c  mq2.c  bh1750.c     ← 步骤1 新建（已存在 FAN.h 保留做兼容，勿删）
├── Middlewares/Services/                     ← 步骤3、5 新建目录（放业务服务层）
│   ├── inc/  config_manager.h  alarm_engine.h  fan_controller.h
│   └── src/  config_manager.c  alarm_engine.c  fan_controller.c
└── Core/Src/  APP_Tasks.c  freertos.c  uart2_comm.c  ← 步骤2/4/5/6/7 修改
```

### 3.2 Keil 工程注册（每个新 .c 都必须做）

在 `MDK-ARM/LVGL_RTOS_DOME5.uvprojx` 中：

1. `mq2.c / bh1750.c / fan_pwm.c` → 加入 **`Module_driver`** 组（与 buzzer/DTH11/FAN 同组，1875 行附近）。
2. 新建 **`Services`** 组，把 `config_manager.c / alarm_engine.c / fan_controller.c` 放进去。
3. **`Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c`** **→ 加入** **`Drivers/STM32F4xx_HAL_Driver`** **组**（当前未注册，不加会链接报错）。
4. 本工程**无** **`.eide`**（非 Bootloader 的 test1 工程），只需注册 Keil 一处。

### 3.3 编码规范

- 中文注释；头文件带 `#ifndef __XXX_H` 防重入 + `extern "C"`（参照现有 FAN.h/DTH11.h）。
- 模块驱动**自包含初始化**：驱动内部自己开时钟、配 GPIO/外设（参照 `FAN_init`/`beep_init` 模式，不要在 main.c 加外设 init）。
- 不使用 `vTaskSuspend/vTaskDelete` 冻结其他任务（沿用 OTA 改造教训），用标志位/队列门控。
- 新建 .c/.h 若含中文注释，**必须带 UTF-8 BOM**（ARMCC 解析无 BOM 中文会报错，项目已有此教训）。

### 3.4 任务与队列参考模板

`lightTask` / `buzzerTask` 是标准模板（UI 回调 → 队列 → 任务消费 → 驱动 + 上报），新任务参照它；AlarmEngineTask 特殊（轮询共享结构体，见步骤 5）。

***

## 4. 分步实施（核心）

***

### 步骤 1：BSP 驱动层（最先写，无依赖）

> 目标：3 个自包含驱动，每个都能独立用串口打印验证。
> **先写 fan\_pwm → 再写 mq2 → 最后写 bh1750**（复杂度递增）。

#### 1.1 fan\_pwm.c/h（替换 FAN.h 的 GPIO 方式）

**先写头文件** **`Drivers/Module_driver/inc/fan_pwm.h`**（参照 FAN.h 骨架）：

```c
#ifndef __FAN_PWM_H
#define __FAN_PWM_H
#include "main.h"

void    FAN_PWM_Init(void);                          // TIM9 + PE5/PE6 复用初始化
void    FAN_PWM_SetDuty(uint8_t fan_id, uint8_t percent); // fan_id:1=PE5,2=PE6; percent:0~100
uint8_t FAN_PWM_GetDuty(uint8_t fan_id);
void    FAN_PWM_Stop(uint8_t fan_id);                // SetDuty(x,0)
void    FAN_PWM_StartMax(uint8_t fan_id);            // SetDuty(x,100)

/* 历史兼容宏（让旧 FAN_ON()/FAN_OFF() 调用仍可用，可选） */
// #define FAN1_ON()  FAN_PWM_StartMax(1)
// #define FAN1_OFF() FAN_PWM_Stop(1)
// #define FAN2_ON()  FAN_PWM_StartMax(2)
// #define FAN2_OFF() FAN_PWM_Stop(2)
#endif
```

**再写源文件** **`Drivers/Module_driver/src/fan_pwm.c`**，依次实现：

1. 静态 `TIM_HandleTypeDef s_htim9;`（本工程 main.h 无 htim9，驱动自持）。
2. `FAN_PWM_Init()`：
   - `__HAL_RCC_TIM9_CLK_ENABLE();` + `__HAL_RCC_GPIOE_CLK_ENABLE();`
   - PE5/PE6 配 `GPIO_MODE_AF_PP`、`GPIO_AF3_TIM9`、`GPIO_SPEED_FREQ_HIGH`、`GPIO_NOPULL`。
   - TIM9 时基：目标计数频率 10 kHz；TIM9 时钟为 168 MHz 时 `PSC = 16800-1`，`ARR = 100-1`，得到 **100 Hz** PWM，占空比 0\~100 直接对应 CCR 0\~100。
   - `HAL_TIM_PWM_Init(&s_htim9)` + `HAL_TIM_PWM_Start(&s_htim9, TIM_CHANNEL_1/2)`。
3. `FAN_PWM_SetDuty(fan_id, percent)`：percent 截断到 0\~100；`__HAL_TIM_SET_COMPARE(&s_htim9, ch, percent)`（ch 由 fan\_id 决定 1→CH1/2→CH2）；percent=0 即停（CCR=0，MOS 全关断）。
4. 用一个 static 数组保存当前占空比供 `GetDuty` 查询。

**最后改** **`FAN.h`（保留兼容层，勿删文件）**：把 `FAN_ON/FAN_OFF/FAN_init` 改写成映射到 `FAN_PWM_*`，或直接改 `main.c` 里 `FAN_init()` 调用为 `FAN_PWM_Init()`。

**验证（此步验收）**：通过报警阈值触发 80% 输出；示波器确认 PE5/PE6 为约 100 Hz、目标占空比正确，两风扇均能起转。更换风扇后重新测最低起转占空比和频率。

#### 1.2 mq2.c/h（ADC1\_IN1）

**先写** **`Drivers/Module_driver/inc/mq2.h`**（参照方案MD 4.1 API）：

```c
#ifndef __MQ2_H
#define __MQ2_H
#include "main.h"
void     MQ2_Init(void);                    // ADC1 + PA1
uint16_t MQ2_ReadRaw(void);                 // 单次 12 位原始值 0~4095
uint16_t MQ2_ReadFiltered(uint8_t samples); // 多次采样取中位数（防尖峰）
#endif
```

**再写** **`Drivers/Module_driver/src/mq2.c`**：

1. 静态 `ADC_HandleTypeDef s_hadc1;`
2. `MQ2_Init()`：
   - `__HAL_RCC_ADC1_CLK_ENABLE();` + `__HAL_RCC_GPIOA_CLK_ENABLE();`
   - PA1 配 `GPIO_MODE_ANALOG`。
   - `s_hadc1.Instance=ADC1;` 时钟 `ADC_CLOCK_SYNC_PCLK_DIV4`（APB2=84MHz→21MHz，满足 ≤36MHz）、分辨率 `ADC_RESOLUTION_12B`、`ADC_SCAN_DISABLE`、单通道；`HAL_ADC_Init`。
   - `HAL_ADC_ConfigChannel`：`ADC_CHANNEL_1`、rank 1、`ADC_SAMPLETIME_15CYCLES`。
3. `MQ2_ReadRaw()`：`HAL_ADC_Start → PollForConversion(短超时) → HAL_ADC_GetValue → HAL_ADC_Stop`。
4. `MQ2_ReadFiltered(samples)`：采 N 次存数组 → 冒泡/插入排序 → 取中位数（方案MD 9.2.3：取中位数不做平均）。

**注意**：Keil 需先注册 `stm32f4xx_hal_adc.c`（见 3.2），否则 `HAL_ADC_*` 链接失败。

**验证**：main 里临时循环打印 `MQ2_ReadFiltered(5)`，打火机气体靠近数值上升；清洁空气回基线。

#### 1.3 bh1750.c/h（软件 I2C，PB6/PB7）

**先写** **`Drivers/Module_driver/inc/bh1750.h`**：

```c
#ifndef __BH1750_H
#define __BH1750_H
#include "main.h"
void    BH1750_Init(void);                      // 软件I2C + 连续H分辨率模式
uint8_t BH1750_ReadLux(uint16_t *out_lux);      // 成功返回 0
#endif
```

**再写** **`Drivers/Module_driver/src/bh1750.c`**：

1. **自带一套软件 I2C 位操作**（宏 + 延时），引脚 **PB6=SCL、PB7=SDA**。**不要复用** `myiic`（那是 PB8/PB9 挂 EEPROM，方案MD 9.1.5 要求 BH1750 独占一条 I2C 总线）。PB10/PB11后来已正式用于USART3外接自动方向TTL-RS485模块，BH1750继续固定使用PB6/PB7。
2. `BH1750_Init()`：GPIOB 时钟 + PB6/PB7 开漏上拉；发 `0x01`（Power On）→ 发 `0x10`（Continuously H-Resolution Mode）→ `delay_ms(180)` 等首次转换。
3. `BH1750_ReadLux()`：I2C 起始 → 写地址 0x46（0x23<<1）→ 读 2 字节（大端）→ `lux=(hi<<8)|lo` → 停止；带 ACK 失败返回非 0。
4. 软件 I2C 有死循环等待，**LightTask 栈给 256 字节起步**（方案MD 9.2.2）。

**验证**：main 临时打印 lux，手电照传感器数值上升、遮住下降。

> 步骤 1 完成后，**3 个驱动的临时测试代码删除或注释掉**，保持工程干净，再进步骤 2。

***

### 步骤 2：数据模型与共享层（纯类型 + 全局变量）

> 目标：先把「数据长什么样」定下来，后面任务/协议/UI 都引用它。**先改头文件，再改 .c 加全局变量。**

**先改** **`Core/Inc/APP_Tasks.h`**：

1. `sensor_Data_t` 扩展（追加字段，注释保留「添加新的传感器数据到这个结构体，蜂鸣器 LED 不属于」）：

```c
typedef struct {
    uint8_t  temp;           // 温度 ℃（已有）
    uint8_t  humi;           // 湿度 %（已有）
    uint8_t  iot_connected;  // 新增：0=离线 1=在线（心跳）
    uint8_t  fan_pct;        // 新增：当前实际风扇占空比 0~100
    uint16_t smoke_adc;      // 新增：MQ-2 12位原始值
    uint16_t lux;            // 新增：BH1750 光照 lx
} sensor_Data_t;
```

1. `UART_MsgType_t` 与 `UART_Msg_t` 扩展：

```c
typedef enum {
    MSG_TYPE_DHT11,
    MSG_TYPE_LED,
    MSG_TYPE_BUZZER,
    MSG_TYPE_FAN,        // 新增：type 0x08 双风扇同步占空比
    MSG_TYPE_SMOKE,      // 新增：type 0x06 烟雾
    MSG_TYPE_LIGHT,      // 新增：type 0x07 光照
} UART_MsgType_t;

typedef struct {
    UART_MsgType_t type;
    union {
        sensor_Data_t dht11_data;
        LED_Cmd_t led_cmd;
        buzzer_Cmd_t buzzer_cmd;
        uint8_t fan_pct;     // 新增：0~100
        uint16_t smoke_adc;  // 新增：12位原始值
        uint16_t lux;        // 新增：lux
    } data;
} UART_Msg_t;
```

1. 新增报警引擎输入结构（可放 APP\_Tasks.h 或 alarm\_engine.h，本文档放 alarm\_engine.h，见步骤 5）。

**再在** **`Core/Src/APP_Tasks.c`** 补全局变量（供 UI 与报警引擎读）：

```c
sensor_Data_t g_dht11_data = {0};   // 共享最新传感器快照（各采集任务写自己字段）
uint8_t       g_iot_connected = 0;  // 心跳在线标志（uart2_comm.c 写，UI 读）
uint32_t      g_last_heartbeat_tick = 0;
```

**最后**：`DHT11Task` 在读到数据后补一行 `g_dht11_data.temp=temp; g_dht11_data.humi=humi;`（其余队列逻辑不动）。

**验收**：编译通过、无回归（温湿度照常显示）。

***

### 步骤 3：配置管理 config\_manager（阈值持久化）

> 依赖：24cxx（已有）。**取消 CRC32**，仅用 `magic` 魔数判有效性。放在 `Middlewares/Services/`。
> **先写结构体与 API → 再写默认值回退。**

**先建** **`Middlewares/Services/inc/config_manager.h`**（结构体与 API 按方案MD 4.4 原样）：

```c
#ifndef __CONFIG_MANAGER_H
#define __CONFIG_MANAGER_H
#include <stdint.h>

typedef struct {
    uint32_t magic;          // 0xA5A5A5A5
    int16_t  temp_high_x10;  // 温度上限 ×10（280=28.0℃）
    int16_t  humi_high;      // 湿度上限 %RH
    uint16_t smoke_adc_thr;  // 烟雾阈值（12位原始值）
    uint16_t light_low;      // 光照下限 lx（低于开灯）
    uint16_t light_high;     // 光照上限 lx（高于关灯，滞回）
    uint16_t reserved;       // 对齐
} app_config_t;              // 共 16 字节（无 CRC32 字段）

void           CFG_Init(void);                       // 读EEPROM，失败用默认值并回写
app_config_t*  CFG_Get(void);                        // 返回只读指针
void           CFG_SetTempHigh(int16_t temp_x10);
void           CFG_SetHumiHigh(int16_t humi);
void           CFG_SetSmokeThr(uint16_t adc_thr);
void           CFG_SetLightRange(uint16_t low, uint16_t high);
void           CFG_SetDefault(void);                 // 恢复默认并写回
uint8_t        CFG_Save(void);                       // 0=OK
#endif
```

**再写** **`Middlewares/Services/src/config_manager.c`**，依次实现：

1. 静态 `static app_config_t g_config;` + 默认值常量（`magic=0xA5A5A5A5, temp_high_x10=280, humi_high=80, smoke_adc_thr=1500, light_low=5, light_high=20`）。
2. `CFG_Init()`：
   - `at24cxx_init();`（幂等）
   - `at24cxx_read(0x70, (uint8_t*)&g_config, sizeof(g_config));`
   - 校验：仅检查 `magic==0xA5A5A5A5`；**不成立 → 填默认值 → CFG\_Save()**（无 CRC 校验）。
3. `CFG_Get()` 返回 `&g_config`。
4. 每个 `CFG_SetXxx()` 只改 RAM 副本；用户按“保存设置”后统一调用一次 `CFG_Save()`。
5. `CFG_Save()`：`at24cxx_write(0x70, (uint8_t*)&g_config, sizeof(g_config));`（16 字节）返回 0。
6. `CFG_SetDefault()`：填默认 → `CFG_Save()`。
7. 若读到旧默认值 `light_low=50, light_high=200`，只迁移为 5/20 并保存，其他配置保持不变。

> EEPROM 地址 **0x70\~0x7F 共 16 字节**，与版本字段 0x2C、OTA 元数据区（0x00\~0x6B）无重叠（方案MD 4.4）。因已删除 CRC 字段，原 0x80\~0x87 的 8 字节不再使用（方案MD 4.4 的 EEPROM 布局表以本文档为准）。

**验收**：上电打印 CFG\_Get() 默认值 → 调用 CFG\_SetTempHigh(300) → 断电重启 → 值仍为 300。

***

### 步骤 4：采集任务 SmokeTask / LightTask

> 依赖：步骤 1 驱动 + 步骤 2 数据模型。参照 `DHT11Task` 模板。
> **先建任务骨架并注册 → 再接驱动采样 → 再写共享全局与报警输入。**

**先改** **`Core/Src/APP_Tasks.c`**：

1. 新增 `SmokeTask`：
   - 优先级 4、栈 `128*2`。
   - `MQ2_Init();` 一次。
   - 循环 `for(;;){ uint16_t adc = MQ2_ReadFiltered(5); g_dht11_data.smoke_adc = adc; ALARM_UpdateInput()->smoke_adc = adc; osDelay(500); }`（500ms 周期）。
2. 新增 `LightTask`：
   - 优先级 3、栈 `128*2`（软件 I2C 栈要多给）。
   - `BH1750_Init();` 一次。
   - 循环：`BH1750_ReadLux(&lux)` 成功则 `g_dht11_data.lux = lux; g_alarm_input.lux = lux;`；`osDelay(1000);`。
3. 在 `AppTasks_Create()` 里用 `xTaskCreate` 注册这两个任务（照抄 DHT11Task 的创建写法）。

**再写报警输入全局**（可先放 alarm\_engine.h 一起，步骤 5 实现后此处编译才通；若先只建任务骨架，可用占位 `extern alarm_input_t g_alarm_input;` 并暂时注释采样写入）。

> ⚠️ 步骤 4 与步骤 5 有轻微耦合（任务要写 `g_alarm_input`）。**推荐顺序：先在步骤 5 建好** **`alarm_engine.h`** **的结构体与** **`g_alarm_input`** **全局，再回来填步骤 4 的采样写入。** 两步骤可交叉，但**必须先有** **`alarm_engine.h`** **的类型定义**。

**验收**：串口每 500ms/1s 打印 ADC 与 lux 数值；任务连续跑不崩（看门狗正常）。

***

### 步骤 5：报警引擎 + 风扇控制（业务核心）

> 依赖：步骤 1\~4 + config\_manager。放在 `Middlewares/Services/`。
> **先写 alarm\_engine.h（类型+全局）→ alarm\_engine.c（决策表）→ fan\_controller.c（调度）→ AlarmEngineTask（组装）。**

#### 5.1 先写 `Middlewares/Services/inc/alarm_engine.h`（按方案MD 4.5）

```c
#ifndef __ALARM_ENGINE_H
#define __ALARM_ENGINE_H
#include <stdint.h>

typedef struct {
    uint16_t smoke_adc;   // 烟雾 ADC 原始值
    int16_t  temp_x10;    // 温度 ×10
    int16_t  humi;        // 湿度 %
    uint16_t lux;         // 光照 lx
} alarm_input_t;

typedef struct {
    uint8_t red_led_on;   // 红灯 PF9 状态（1=闪亮相位）
    uint8_t buzzer_on;    // 蜂鸣器 PF8 状态
    uint8_t fan_pct;      // 风扇占空比 0~100（双风扇同步）
    uint8_t green_led_on; // 绿灯 PF10 状态
    uint8_t alarm_level;  // 0=无 1=温湿度 2=烟雾
} alarm_output_t;

extern alarm_input_t  g_alarm_input;    // 采集任务写
extern alarm_output_t g_alarm_output;   // AlarmEngine 算
void ALARM_Update(void);                // 读 g_alarm_input → 决策表 → 写 g_alarm_output
#endif
```

#### 5.2 再写 `Middlewares/Services/src/alarm_engine.c`

实现 `ALARM_Update()`（每 200ms 调用），**先写阈值判断 → 再写去抖 → 最后写声光相位**：

1. 读 `g_alarm_input` 与 `CFG_Get()`。
2. **去抖**（方案MD 9.2.4，2s = 10 tick）：
   - static 计数器：`smoke_over_cnt`、`temp_over_cnt`、`humi_over_cnt`。
   - 条件满足则 `cnt++`（封顶），不满足则 `cnt=0`；`cnt>=10` 才判定「超限成立」。
3. **烟雾（LEVEL2）**：`smoke_adc > smoke_adc_thr` 且去抖成立 → `alarm_level=2`，`fan_pct=100`。
4. **温湿度（LEVEL1）**：`temp_x10 > temp_high_x10` 或 `humi > humi_high`（任一，去抖成立）→ `alarm_level=1`，`fan_pct=80`（当前固定值）。
5. 优先级：烟雾 > 温湿度（先判烟雾，命中则不再走 LEVEL1）。
6. **光照（独立链路，与报警状态机无关）**：`lux < light_low` → 绿灯亮；`lux > light_high` 且前态为亮 → 灭（滞回，方案MD 4.5）。
7. **声光相位**（在 AlarmEngineTask 200ms 循环里用 `HAL_GetTick()` 算，不阻塞）：
   - 红灯 2Hz 快闪：`(HAL_GetTick()/250)%2` 决定亮/灭（仅 LEVEL2）。
   - 蜂鸣器 1Hz 滴答：`(HAL_GetTick()/500)%2`，且用 200ms 开/800ms 关的占空比相位（方案MD 决策表「200ms开+800ms关」→ `(tick%1000)<200`）。
   - 绿灯按第 6 步结果直接亮/灭。
8. 全部正常：`alarm_level=0, fan_pct=0`，红灯灭、蜂鸣器关、绿灯按光照。

#### 5.3 再写 `Middlewares/Services/inc/fan_controller.h` + `src/fan_controller.c`（按方案MD 4.6）

```c
#ifndef __FAN_CONTROLLER_H
#define __FAN_CONTROLLER_H
#include <stdint.h>
void FANCTRL_Init(void);
void FANCTRL_UpdateFromAlarm(uint8_t pct);                    // 报警引擎输出
void FANCTRL_Run(void);                                       // 周期仲裁 + 写 PWM
uint8_t FANCTRL_GetActualDuty(void);                          // 读取已写入的共同占空比
#endif
```

实现要点（保持最简单的单点写入）：

1. static 状态只保留 `s_alarm_target_pct` 和 `s_actual_pct`。
2. `FANCTRL_UpdateFromAlarm(pct)` 保存 AlarmEngine 给出的 0/80/100% 目标。
3. `FANCTRL_Run()` 仅在目标变化时同步写 PE5/PE6 两路 PWM，并更新实际值。
4. `FANCTRL_GetActualDuty()` 给 UI 和 UART 上报读取；不实现手动风扇和 PI 占位函数。

#### 5.4 最后在 `Core/Src/APP_Tasks.c` 加 `AlarmEngineTask` 并注册

1. `AlarmEngineTask`：优先级 5、栈 `128*4`。
2. 循环（每 200ms）：
   - `ALARM_Update();` → 计算 `g_alarm_output`。
   - `FANCTRL_UpdateFromAlarm(g_alarm_output.fan_pct); FANCTRL_Run();`
   - 按 `g_alarm_output` 写 红灯 PF9 / 蜂鸣器 PF8 / 绿灯 PF10（红灯低电平点亮，参照 lightTask 的 `GPIO_PIN_RESET`；蜂鸣器高电平响，参照 buzzerTask 的 `GPIO_PIN_SET`；绿灯 PF10 先确认已在某驱动初始化，没有则在任务里 `led` 相关 GPIO init 或新增初始化）。
   - 每 25 个周期（5s）：**周期上报** —— 拼 `UART_Msg_t` 发 `xUART2_TxQueue`：`MSG_TYPE_SMOKE`(g\_alarm\_input.smoke\_adc)、`MSG_TYPE_LIGHT`(g\_alarm\_input.lux)、`MSG_TYPE_FAN`(g\_dht11\_data.fan\_pct)。
3. `AppTasks_Create()` 里注册（**不要**新建 fanTask；UI 风扇 bar 保持只读显示）。

**验收**：临时把烟雾阈值调低（或对 MQ-2 吹气）→ 红灯 2Hz 快闪 + 蜂鸣器 1Hz 滴答 + 双风扇 100%；温度阈值调低 → 风扇 80% 无声光；遮光 → 绿灯亮。

***

### 步骤 6：UART2 物模型扩展（0x06/0x07/0x08 上报 + 0x0A 心跳解析）

> 依赖：步骤 2 的 `UART_Msg_t`。**先改发送（QueueBusinessPackets）→ 再改接收（ProcessCompletedFrame）。**

**先改** **`Core/Src/uart2_comm.c`**：

1. `UART2_Comm_QueueBusinessPackets()` 的 switch 增加三个 case（参照 MSG\_TYPE\_LED 的写法）：

```c
case MSG_TYPE_SMOKE:
    UART2_Comm_InitPacket(&packet, 0x06U);
    packet.data[0] = (uint8_t)(msg.data.smoke_adc >> 8);  // 高字节
    packet.data[1] = (uint8_t)(msg.data.smoke_adc & 0xFF); // 低字节
    UART2_Comm_QueuePacket(&packet);
    break;
case MSG_TYPE_LIGHT:
    UART2_Comm_InitPacket(&packet, 0x07U);
    packet.data[0] = (uint8_t)(msg.data.lux >> 8);
    packet.data[1] = (uint8_t)(msg.data.lux & 0xFF);
    UART2_Comm_QueuePacket(&packet);
    break;
case MSG_TYPE_FAN:
    UART2_Comm_InitPacket(&packet, 0x08U);   // 双风扇同步单指令
    packet.data[0] = msg.data.fan_pct;        // 0~100
    UART2_Comm_QueuePacket(&packet);
    break;
```

1. **`UART2_Comm_ProcessCompletedFrame()`** **增加 0x0A 心跳 case**（插在「版本请求分支 0x09」之后、最后的 `else [UART2][RX][DROP]` 之前，作为一个新的 `else if`；当前函数内分支顺序为 ACK→OTA 0x01→版本 0x09→else，勿插到 else 之后）：

```c
else if ((s_rx_expected_length == sizeof(UART_Packet_t)) &&
         (s_rx_frame[1] == 0x0AU) &&
         (s_rx_frame[sizeof(UART_Packet_t)-1] == UART2_COMM_BUSINESS_TAIL))
{
    /* ESP32→STM32 心跳：55 0A status 00 00 00 00 BB */
    g_iot_connected = (s_rx_frame[2] != 0U) ? 1U : 0U;
    g_last_heartbeat_tick = HAL_GetTick();
    UART2_Comm_LogFrame("[UART2][RX][HEARTBEAT]", s_rx_frame, s_rx_expected_length);
}
```

> 说明：`g_iot_connected` / `g_last_heartbeat_tick` 在 APP\_Tasks.c 定义（步骤 2），此处用 `extern` 引用；或在本文件定义、头文件 extern（二选一，保持一致）。

**协议约定（写进注释与同步更新方案MD/TXT）**：

| type | 方向          | data\[5]         | 说明                     |
| ---- | ----------- | ---------------- | ---------------------- |
| 0x06 | STM32→ESP32 | \[0]=ADC高 \[1]=低 | 烟雾                     |
| 0x07 | STM32→ESP32 | \[0]=lux高 \[1]=低 | 光照                     |
| 0x08 | STM32→ESP32 | \[0]=占空比0\~100   | 风扇（双风扇同步，**删除原 0x09**） |
| 0x0A | ESP32→STM32 | \[0]=0离线/1在线     | 心跳，5s 周期               |
| 0x85 | （预留，初版不做）   | \[0]=0\~100      | 手动风扇（合并原 0x86）         |

**验收**：ESP32 `idf.py monitor` 能看到 0x06/0x07/0x08 新帧且 ACK 正常；用 ESP32 发 `55 0A 01 00 00 00 00 BB`，STM32 调试串口打印 `[UART2][RX][HEARTBEAT]` 且 `g_iot_connected=1`。

***

### 步骤 7：UI 联动（数值刷新 + 设置屏阈值 + IOT\_FLAG）

> 依赖：步骤 3/5/6 的全局量。**先刷新首页数值 → 再 IOT\_FLAG → 最后设置屏阈值。**

#### 7.1 首页数值刷新（`Core/Src/freertos.c` 的 `StartDefaultTask`）

在 `for(;;)` 里新增 500ms 节流块（**温湿度队列逻辑不动**）：

```c
static uint32_t last_ui_refresh = 0;
if (HAL_GetTick() - last_ui_refresh >= 500)
{
    last_ui_refresh = HAL_GetTick();
    lv_label_set_text_fmt(guider_ui.screen_smoke_num, "%u", g_dht11_data.smoke_adc);
    lv_label_set_text_fmt(guider_ui.screen_sunlight_num, "%u lx", g_dht11_data.lux);
    lv_label_set_text_fmt(guider_ui.screen_fan_num, "%d%%", g_dht11_data.fan_pct);

    /* IOT 心跳：15s 未收到判离线 */
    if (HAL_GetTick() - g_last_heartbeat_tick > 15000) g_iot_connected = 0;
    lv_label_set_text(guider_ui.screen_iot_flag, g_iot_connected ? "ON" : "NC");
    lv_obj_set_style_text_color(guider_ui.screen_iot_flag,
                                lv_color_hex(g_iot_connected ? 0x0ac75d : 0xe74c3c),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
}
```

#### 7.2 设置屏阈值（回调放 `APP_Tasks.c`，注册放 `freertos.c`）

> 不修改 generated 的 `events_init.c`（重新导出会覆盖）。仿照现有 `screen_sw_light_event_cb` 的模式，把业务回调放 APP\_Tasks.c，在 `StartDefaultTask` 里 `lv_obj_add_event_cb` 注册（可先判 `NULL` 防控件缺失）。

**先写回调（`APP_Tasks.c`）**，共 5 类：

1. **全局暂存** `static int16_t s_edit_temp, s_edit_humi, s_edit_smoke, s_edit_light_low, s_edit_light_high;` + `static uint8_t s_edit_loaded=0;`（首次从 `CFG_Get()` 装载）。
2. `screen_setting_add_1/minus_1` 回调：温度 `s_edit_temp ± 1`，范围 -10\~60，更新 `screen_setting_num_temp`（显示整数 ℃）。
3. `screen_setting_add_2/minus_2` 回调：湿度 `s_edit_humi ± 1`，范围 20\~100，更新 `screen_setting_num_hum`。
4. `screen_setting_add_3/minus_3` 回调：光照上限 `s_edit_light_high ± 10`，更新 `screen_setting_num_sunlight`。
   > ⚠️ **光照下限** **`s_edit_light_low`** **在生成 UI 中没有独立控件**（设置屏只有一组 `add_3/minus_3`+`num_sunlight` 对应 `light_high`）。初版做法：`s_edit_light_low` 从 `CFG_Get()` 装载、不在 UI 编辑、保存时原样写回（默认 5 lx）。**不要**为它新增控件或加额外回调。
5. `screen_setting_add_4/minus_4` 回调：烟雾 `s_edit_smoke ± 10`，范围 0\~4095，更新 `screen_setting_num_smoke`。
6. `screen_setting_btn_save` 回调：`CFG_SetTempHigh(s_edit_temp*10); CFG_SetHumiHigh(s_edit_humi); CFG_SetSmokeThr(s_edit_smoke); CFG_SetLightRange(s_edit_light_low, s_edit_light_high);`（SetXxx 内部自动写 EEPROM）。
7. `screen_setting_btn_default` 回调：`CFG_SetDefault();` 重新装载暂存并刷新全部 num label。
8. `screen_setting_btn_reset` 回调：**初版留空**（延后功能，见第 6 节）。`screen_setting_btn_Version`：可显示版本或留空（可选）。

**再注册（`freertos.c`** **StartDefaultTask 回调区追加）**：

```c
lv_obj_add_event_cb(guider_ui.screen_setting_add_1, screen_setting_add_1_cb, LV_EVENT_CLICKED, NULL);
lv_obj_add_event_cb(guider_ui.screen_setting_minus_1, screen_setting_minus_1_cb, LV_EVENT_CLICKED, NULL);
/* ... add_2/minus_2/add_3/minus_3/add_4/minus_4 同型注册 ... */
lv_obj_add_event_cb(guider_ui.screen_setting_btn_save, screen_setting_btn_save_cb, LV_EVENT_CLICKED, NULL);
lv_obj_add_event_cb(guider_ui.screen_setting_btn_default, screen_setting_btn_default_cb, LV_EVENT_CLICKED, NULL);
```

> 进设置屏时显示当前阈值：可在上面 7.1 的 500ms 块里，仅当 `lv_scr_act()==guider_ui.screen_setting` 时刷新 4 个 num label（或首次 `s_edit_loaded` 时刷新一次）。

**验收**：进设置屏 → +/- 调阈值 → 保存 → 断电重启 → 值不变；首页 smoke/lux/fan/iot 每 500ms 刷新正常。

***

### 步骤 8：ESP32 端（心跳 + 新增属性上报）

> 先处理 OneNET 云平台物模型属性 → 再改代码。**IOT 在线状态不加 OneNET 属性**：心跳是 ESP32→STM32 的 UART 本地信号，只用于 STM32 屏幕显示，不需上云。

#### 8.1 OneNET 物模型（已完成，本步骤仅需发布生效）

用户已在 OneNET 控制台完成 7 个属性配置（见产品开发→物模型→属性），发布量产即生效：

| 功能名称 | 标识符 | 数据类型 | 读写类型 | 代码侧 key（`onenet_dm.c`） |
| --- | --- | --- | --- | --- |
| 温度 | `temp` | int32 | 读写 | ✅ 已有 |
| 湿度 | `hum` | int32 | 读写 | ✅ 已有 |
| 照明灯 | `LED` | bool | 读写 | ✅ 已有 |
| 湿度报警器 | `Alarm` | bool | 读写 | ✅ 已有 |
| **烟雾浓度** | **`Smoke`** | int32 | 只读 | ❌ 待新增 |
| **光照强度** | **`Lux`** | int32 | 只读 | ❌ 待新增 |
| **风扇占空比** | **`Fan`** | int32 | 只读 | ❌ 待新增 |

**必须动作**：OneNET 控制台 → 物模型页面 → 点「下一步」走完发布流程（设备开发→设备调试→发布量产），让新属性在云端生效。

#### 8.2 新增属性上报代码（改 `ESP32S3-OTA/main/iot/onenet_dm.c`）

在 `onenet_property_upload_dm()` 函数中，现有 `temp/hum/LED/Alarm` 上报代码**不动**，在 `Alarm` 块之后追加 3 段：

```c
/* 烟雾浓度 */
cJSON* Smoke = cJSON_AddObjectToObject(params_js,"Smoke");
cJSON_AddNumberToObject(Smoke,"value", smoke_adc);      // 从 STM32 0x06 帧解析得来
ESP_LOGI(TAG,"Smoke value:%d",smoke_adc);

/* 光照强度 */
cJSON* Lux = cJSON_AddObjectToObject(params_js,"Lux");
cJSON_AddNumberToObject(Lux,"value", lux);              // 从 STM32 0x07 帧解析得来
ESP_LOGI(TAG,"Lux value:%d",lux);

/* 风扇占空比 */
cJSON* Fan = cJSON_AddObjectToObject(params_js,"Fan");
cJSON_AddNumberToObject(Fan,"value", fan_pct);          // 从 STM32 0x08 帧解析得来
ESP_LOGI(TAG,"Fan value:%d",fan_pct);
```

**同步处理**：UART2 接收任务里 `ProcessCompletedFrame` 扩展 0x06/0x07/0x08 帧解析（见步骤 6），将 STM32 发来的数据存入 `smoke_adc/lux/fan_pct` 变量供 `property_report_task` 读取。

#### 8.3 心跳发送（STM32 屏幕用，不上云）

ESP32 端在现有 OTA/OneNET 工程中加一个轻量周期任务（或并入现有 `property_report_task`）：

1. 每 5s 读取 `onenet_is_connected()`（`onenet_mqtt.c` 已就绪）。
2. 组帧 `55 0A <status> 00 00 00 00 BB`（status=1 在线 / 0 离线），经 UART 发给 STM32（与现有业务帧同一发送通路）。
3. 无 OneNET 连接时也照发（status=0），保证 STM32 心跳超时逻辑可测。

**验收**：
- OneNET 控制台能看到 7 个属性实时刷新（含 Smoke/Lux/Fan 三个新值）
- STM32 `screen_iot_flag` 由红 NC → 绿 ON；断开 WiFi/云后 ≤15s 变回 NC

***

### 步骤 9：联调验证与收尾

1. **Keil 注册检查**（见 3.2）：3 个驱动 + 3 个服务 + `stm32f4xx_hal_adc.c` 全部在工程里。
2. **编译**：`Rebuild` → 0 错误（新 .c/.h 中文注释确保带 BOM）。
3. **烧录/OTA 前**：确认 `ota_storage.c` 的测试注入（如固件 CRC 注入 `//addr += 1;`）已删除。
4. **版本号**：`Core/Src/main.c` 里 `printf("...v1.0.16...")` 更新为 `v1.1.0`；确认 Bootloader OTA 时写入 AT24C02\@0x2C 的版本字段与之一致（保证 ESP32 上报 OneNET 的版本闭环，方案MD 9.3.2）。
5. **全链路测试用例**（按方案MD S1\~S5 验收标准）：
   - 用例1：正常升级回归（Bootloader→APP→版本上报）。
   - 用例2：三个驱动串口打印数值正常。
   - 用例3：烟雾触发（吹 MQ-2 或临时调低阈值）→ 红灯 2Hz + 蜂鸣器 1Hz + 双风扇 100%，且 ESP32 收到 0x06 帧。
   - 用例4：温度超限 → 风扇 80% 无声光，收到 0x08 帧占空比 80。
   - 用例5：遮光 → 绿灯亮，收到 0x07 帧。
   - 用例6：设置屏改阈值保存 → 断电重启不丢。
   - 用例7：ESP32 上线 → IOT\_FLAG 绿 ON；断网 ≤15s → 红 NC。
6. **协议文档同步**：把 `A区APP功能扩展方案.md` 第六章、`ESP32S3_STM32F407_引脚清单与通信协议.txt` 第八章同步为本文档 0.3/步骤 6 的协议（0x08 单指令、删 0x09/0x86、加 0x0A 心跳）。**两处文档必须与代码一致。**

**2026-10-17 联调结果**：原用例2、用例4、用例5和配置保存路径已由BOSS烧录验证；风扇80%的报警链路与100 Hz波形已验证。SHT30正式业务代码已确认正常通讯和正常显示，正式版v2.0.0 OTA版本闭环也已回归通过。MQ-2风扇干扰、BH1750长期变化及未保存完整日志的RS485故障场景仍按验证边界记录，不能笼统写成全部验收通过。

***

## 5. 注意事项与坑（汇总，编码时逐条对照）

1. **`stm32f4xx_hal_adc.c`** **必须注册进 Keil**，否则 `HAL_ADC_*` 链接失败（hal\_tim 已注册，ADC 没有）。
2. **config\_manager 不写 CRC32**（本次简化）：仅以 `magic==0xA5A5A5A5` 判数据有效性；`app_config_t` 共 16 字节、EEPROM 0x70\~0x7F 写满即可。
3. **BH1750用PB6/PB7独立软件I2C**，勿复用myiic（PB8/PB9挂EEPROM）；PB10/PB11已正式用于USART3外接自动方向TTL-RS485模块。
4. **风扇改线 PE5/PE6（TIM9）**，PG14/PG15 无 TIM 复用不能 PWM；当前两线风扇实测使用 100 Hz，10 kHz 会只响不转。
5. **声光闪烁用 HAL\_GetTick() 相位**，不要在 200ms 任务里 osDelay 阻塞；去抖用 tick 计数（2s=10 tick）。
6. **不要新增 fanTask 队列**：Alarm 走 `FANCTRL_UpdateFromAlarm`，`FANCTRL_Run()` 单点写 PWM，UI 只读最终值，避免双写冲突。
7. **上报流量**：UART2 是串行 ACK 模式（200ms 超时/帧，最多 4 发），周期上报 5s 一次 3 帧，勿按采样周期刷屏式上报。
8. **EEPROM 写入寿命**：`CFG_Save()` 只在用户点保存/默认时写，**禁止在采集循环里调用**（方案MD 9.2.5）。
9. **禁止 vTaskSuspend/vTaskDelete** 冻结 UART/其他任务；用标志位/队列门控（OTA 改造教训）。
10. **新文件中文注释带 UTF-8 BOM**，否则 ARMCC 解析报错。
11. **`screen_bar_fan` 只显示最终占空比**：不要再把它接成手动输入，`g_fan_pwm` 由 AlarmEngineTask 写入实际输出值。
12. **心跳超时 15s**（3×5s）；`g_last_heartbeat_tick` 初始 0，上电后先显示 NC 属正常。
13. **必须共地**：STM32、外部电源、传感器和风扇驱动共地；模块 VCC 可由外部电源供电，但不同 VCC 电源轨不要直接并联。
14. **MQ-2 与风扇存在实机耦合**：风扇转动时偶发 4095，停扇后消失；先测供电、地线和 PA1/AO，再决定硬件滤波或软件抗干扰。
15. **BH1750 遮挡响应正常不等于长期稳定**：遮挡可接近 0，基本 I2C 链路正常；固定光源下缓慢上升仍需单独对照测试。
16. **SHT30精度和协议分层**：STM32内部及LVGL保留x10定点数和1位小数；UART2发ESP32前四舍五入成整数，不修改现有0x01/0x02及OneNET物模型。
17. **SHT30故障退化必须回归**：断电、A/B接反或无响应时清温湿度有效标志，UI显示`NC`，报警引擎约2秒去抖退出旧温湿度状态。

***

## 6. 初版延后事项与当前状态

以下内容最初均为延后方案；RS485替换已经完成，其余项目仍不要混入当前最终收尾：

1. **RS485 Modbus RTU替换DHT11：已完成正式接入**。PB10/PB11使用USART3，外接自动方向TTL-RS485模块；已有CRC、超时返回和有效标志，自动重试采用下一个1秒采集周期。正常链路及正式版v2.0.0 OTA已确认，未保存完整日志的断线场景继续按边界记录。
2. **PI环境量闭环：继续延后**。先完成RS485故障和全链路回归，再用温度/湿度偏差调节风扇占空比；加入输出限幅、最低运行占空比、启动助推和积分抗饱和。烟雾100%仍最高优先，不加D。
3. **手动复位报警 + 120s 冷却期**（原 v2 附录 A）——排在 RS485 与 PI 之后。
4. **OneNET 云平台下发阈值（0x8X 控制帧）**。
5. **报警历史记录（SRAM 环形缓冲）**。
6. **ESP32-S3 自身 A/B 分区 OTA**。

***

*本文档与* *`A区APP功能扩展方案.md`* *配套，方案MD 描述设计，本文档驱动编码实施。协议号与目录路径以本文档为准，如需改动请同步更新方案MD 与* *`ESP32S3_STM32F407_引脚清单与通信协议.txt`。*
