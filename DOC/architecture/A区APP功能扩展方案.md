# STM32 A区 APP 功能扩展方案

> 适用项目：ESP32-S3 + STM32F407ZGT6 双MCU OTA智能环境监测系统
> 版本：v1.2（SHT30正式接入修订）
> 日期：2026-10-17
> 作者：小绿同学 × BOSS

> 当前状态：S1~S5和SHT30 Modbus替换已完成，正式版v2.0.0双端OTA回归已由BOSS实机确认；当前进入发布记录、素材和Git稳定基线整理。

***

## 一、方案总览

### 1.1 扩展目标

在现有 A 区 APP（温湿度采集 + LVGL UI + ESP32 OTA 物模型上报）基础上，新增 **烟雾监测、光照监测、双风扇智能控制、声光报警、本地阈值配置** 五项功能，形成完整的智能环境监测与告警闭环。

### 1.2 新增硬件模块

| 模块    | 型号                         | 接口                 |  数量 | 功能                    |
| ----- | -------------------------- | ------------------ | :-: | --------------------- |
| 烟雾传感器 | MQ-2                       | ADC（模拟量输出）         |  1  | 检测可燃气体/烟雾浓度           |
| 光照传感器 | GY-302 BH1750FVI           | I2C（ADDR接地=地址0x23） |  1  | 检测环境光照强度（0\~65535 lx） |
| 风扇模块  | 30×30mm 5V + AO3400 MOS管驱动 | PWM（低边N-MOS）       |  2  | 双风扇独立控制，排烟/降温/排湿      |
| 温湿度采集器 | SHT30 RS485模块 + 自动方向TTL-RS485 | USART3 / Modbus RTU | 1 | 采集0.1精度温湿度，替换DHT11 |

> **注意**：BH1750 GY-302 模块标准为 I2C 接口（VCC/GND/SCL/SDA/ADDR 5脚），原文档"SPI控制"系笔误。AO3400 栅极接 FAN\_IN，支持 PWM 开关调速。

### 1.3 Flash 资源评估（STM32F407ZGT6 1MB）

| 项目                                         | 占用           |
| ------------------------------------------ | ------------ |
| Bootloader（Sector 0-3）                     | 64 KB        |
| 当前A区 APP 固件（R0日志）                          | \~475 KB     |
| 新增功能预估（驱动+服务+UI）                           | \~50-80 KB   |
| **预计总A区固件大小**                              | **\~550 KB** |
| A区最大可用（Sector 4-11 = 128KB + 64KB×6+128KB） | \~512KB+     |
| **Flash剩余空间**                              | **充足 ✅**     |

> 实际 A 区大小以 Keil `Target → Read/Only Memory Areas` 设置为准。1MB 总 Flash 扣除 64KB Bootloader 后，理论可用 960KB。

### 1.3 OneNET 云平台物模型属性（STM32→ESP32→OneNET 上报）

Product ID 和设备名从本地 `credentials.h` 读取，不写入公开文档。物模型共 7 个属性，全部为设备上报方向（初版不做云端下发）：

| 功能名称 | 标识符 | 数据类型 | 读写类型 | 说明 |
| --- | --- | --- | --- | --- |
| 温度 | `temp` | int32 | 读写 | 已有 |
| 湿度 | `hum` | int32 | 读写 | 已有 |
| 照明灯 | `LED` | bool | 读写 | 已有 |
| 湿度报警器 | `Alarm` | bool | 读写 | 已有（蜂鸣器状态） |
| **烟雾浓度** | **`Smoke`** | int32 | 只读 | **新增** ADC 原始值 0~4095 |
| **光照强度** | **`Lux`** | int32 | 只读 | **新增** BH1750 lux 0~65535 |
| **风扇占空比** | **`Fan`** | int32 | 只读 | **新增** 双风扇同步占空比 0~100% |

> ⚠️ **IOT 在线状态不上云**：心跳帧 `0x0A` 是 ESP32→STM32 的 UART 本地信号，仅用于 STM32 屏幕 `screen_iot_flag` 显示 ON/NC，不通过 OneNET 属性上报。
> 标识符必须与 ESP32 端 `onenet_dm.c` 的 `cJSON_AddObjectToObject(params_js, "<key>")` 字符串完全一致，否则平台拒绝上报。

***

## 二、引脚分配（最终方案）

详见 `ESP32S3_STM32F407_引脚清单与通信协议.txt` 第四节 A 区应用引脚。

| 模块                 | 引脚   | 复用功能         | 说明                             |
| ------------------ | ---- | ------------ | ------------------------------ |
| **风扇1 PWM**        | PE5  | TIM9\_CH1    | AO3400 栅极 FAN\_IN1，原 PG14 改接至此 |
| **风扇2 PWM**        | PE6  | TIM9\_CH2    | AO3400 栅极 FAN\_IN2，原 PG15 改接至此 |
| **MQ-2 烟雾 ADC**    | PA1  | ADC1\_IN1    | 12位 0\~4095 原始值                |
| **BH1750 I2C SCL** | PB6  | GPIO 软件 I2C  | GY-302 SCL（P9 排针第2脚），ADDR已接GND        |
| **BH1750 I2C SDA** | PB7  | GPIO 软件 I2C  | GY-302 SDA（P9 排针第3脚），ADDR已接GND→I2C地址0x23 |
| **SHT30 Modbus TX** | PB10 | USART3\_TX | 接外部自动方向TTL-RS485模块RXD，9600 8N1 |
| **SHT30 Modbus RX** | PB11 | USART3\_RX | 接外部自动方向TTL-RS485模块TXD，9600 8N1 |

### 2.1 硬件跳线变更

| 原引脚       | 新引脚 | 变更原因                                 |
| --------- | --- | ------------------------------------ |
| PG14（风扇1） | PE5 | PG14 无定时器复用，无法输出 PWM；PE5 = TIM9\_CH1 |
| PG15（风扇2） | PE6 | PG15 无定时器复用，无法输出 PWM；PE6 = TIM9\_CH2 |
| PB10（BH1750 SCL，方案原定） | PB6 | PB10/PB11用于USART3外接自动方向TTL-RS485模块，SHT30 Modbus已正式启用，故BH1750改接PB6/PB7 |
| PB11（BH1750 SDA，方案原定） | PB7 | 同上 |

### 2.2 风扇驱动电路确认

根据原理图识别结果：

- **拓扑**：低边 N-MOS 驱动（AO3400）
- **栅极 FAN\_IN**：通过 10K 下拉电阻 R35 到地 → 默认关风扇
- **漏极 D**：接风扇 CN1.2（风扇负极）
- **源极 S**：接 GND
- **PWM 可行性**：AO3400 开关链路已通过示波器确认，但风扇能否正常调速还取决于两线无刷风扇内部驱动。当前风扇在 10 kHz、80% 时只响不转，在 100 Hz、80% 时可正常起转，因此本工程锁定 100 Hz

***

## 三、分层架构设计

遵循项目现有「伪操作系统分层」风格，新增代码按以下层次放置：

```
STM32-OTA/A区智能哨兵环境监测系统/
├── Drivers/Module_driver/
│   ├── inc/
│   │   ├── mq2.h              ← 新增：MQ-2 烟雾 ADC 驱动接口
│   │   ├── bh1750.h           ← 新增：BH1750 光照 I2C 驱动接口
│   │   ├── fan_pwm.h          ← 新增：双风扇 PWM 驱动接口（替换原 FAN.h 的 GPIO 方式）
│   │   ├── FAN.h              ← 保留：仅历史兼容宏，实际调用走 fan_pwm
│   │   └── ...
│   └── src/
│       ├── mq2.c              ← 新增：ADC1 CH1 初始化 + 阻塞/非阻塞采样 API
│       ├── bh1750.c           ← 新增：软件 I2C(PB6/PB7) 初始化 + 连续模式读数
│       ├── fan_pwm.c          ← 新增：TIM9 CH1/CH2 初始化 + 双路独立占空比设置
│       └── ...
├── Middlewares/Services/
│   ├── inc/
│   │   ├── alarm_engine.h     ← 新增：报警引擎（优先级/联动/状态机）
│   │   ├── config_manager.h   ← 新增：AT24C02 阈值配置存取（0x70 起 16B，magic 校验）
│   │   ├── fan_controller.h   ← 新增：风扇控制抽象（报警目标/单点输出）
│   │   └── ...
│   └── src/
│       ├── alarm_engine.c     ← 新增：三级优先级状态机 + 执行器输出计算
│       ├── config_manager.c   ← 新增：app_config_t 读写 + magic 校验 + 默认值回退（无 CRC32）
│       └── fan_controller.c   ← 新增：双风扇占空比调度（0/80/100% 同步输出）
└── Core/Src/
    ├── APP_Tasks.c            ← 修改：新增 SmokeTask/LightTask/AlarmEngineTask
    ├── APP_UserInterface.c    ← 修改：新增阈值设置页 / 报警状态页 / 风扇状态显示
    ├── UART2_Comm.c           ← 修改：扩展物模型 type 0x06/0x07/0x08 上报 + 0x0A 心跳解析
    └── ...
```

***

## 四、软件模块详细设计

### 4.1 MQ-2 烟雾驱动（mq2.c/h）

**初始化**：

- ADC1 开启，配置通道 PA1，12 位分辨率，单次转换模式
- 采样时间建议 15 个时钟周期（480KHz 采样率足够）
- 可选：DMA 模式 + 多通道（若后续扩展更多模拟量）

**API**：

```c
void     MQ2_Init(void);                              // ADC1 + PA1 GPIO 初始化
uint16_t MQ2_ReadRaw(void);                           // 单次采样返回 12 位原始值 0~4095
uint16_t MQ2_ReadFiltered(uint8_t samples);           // 多次采样取中位数（防抖动）
// 可选后续扩展：
// float MQ2_RawToPpm(uint16_t raw);                  // 原始值→ppm（需标定曲线，暂不实现）
```

**FreeRTOS 任务周期**：SmokeTask 每 500ms 采样一次，取中位数后存入全局结构。

***

### 4.2 BH1750 光照驱动（bh1750.c/h）

**接口**：软件 I2C，SCL=PB6，SDA=PB7，I2C 地址 0x23（ADDR 已接地）。

**操作模式**：连续 H-Resolution 模式（分辨率 1 lx，典型转换时间 120 ms）。

**初始化序列**：

1. 发送 Power On 命令（0x01）
2. 发送 Continuously H-Resolution Mode 命令（0x10）
3. 等待 ≥180 ms 首次转换
4. 每次读取：I2C 读 2 字节，大端序 lux = (high<<8)|low

**API**：

```c
void    BH1750_Init(void);                            // 软件 I2C + 连续模式初始化
uint8_t BH1750_ReadLux(uint16_t *out_lux);            // 读当前照度，返回HAL_OK/ERROR
```

**FreeRTOS 任务周期**：LightTask 每 1s 采样一次。

***

### 4.3 双风扇 PWM 驱动（fan\_pwm.c/h，替换原 FAN.h GPIO 方式）

**定时器配置**：

- TIM9 通用定时器（APB2 总线，168 MHz）
- 目标计数频率 10 kHz；TIM9 时钟为 168 MHz 时，PSC = 16800-1
- 自动重装 ARR = 100-1，PWM 频率 = 10 kHz / 100 = **100 Hz**
- CH1=PE5 PWM1 模式，CH2=PE6 PWM1 模式
- 占空比范围 = CCR / ARR = 0\~100 对应 0%\~100%

**API**：

```c
void     FAN_PWM_Init(void);                          // TIM9 + GPIO 复用初始化
void     FAN_PWM_SetDuty(uint8_t fan_id, uint8_t percent);  // fan_id=1/2, percent=0~100
uint8_t  FAN_PWM_GetDuty(uint8_t fan_id);             // 查询当前占空比
void     FAN_PWM_Stop(uint8_t fan_id);                // 等价于 SetDuty(x, 0)
void     FAN_PWM_StartMax(uint8_t fan_id);            // 等价于 SetDuty(x, 100)
// 历史兼容宏（FAN.h 保留，映射到新 API）：
// #define FAN1_ON()   FAN_PWM_StartMax(1)
// #define FAN1_OFF()  FAN_PWM_Stop(1)
// #define FAN2_ON()   FAN_PWM_StartMax(2)
// #define FAN2_OFF()  FAN_PWM_Stop(2)
```

**注意**：

- percent > 100 时自动截断为 100
- 0% 占空比时建议关闭 PWM 输出通道（或直接 CCR=0，MOS 管完全关断）
- 当前两线风扇已经实测 80% 可起转；更换风扇后必须重新测最低起转占空比和合适 PWM 频率

***

### 4.4 配置管理服务（config\_manager.c/h）

**目的**：将阈值持久化到 AT24C02，断电不丢失，用 magic 魔数判断数据有效性（**取消 CRC32 自校验**，简化实现）。

**EEPROM 布局**（0x70 起，16 字节，**取消 CRC32 校验**）：

| 偏移   | 类型        | 字段              | 默认值        | 说明                       |
| ---- | --------- | --------------- | ---------- | ------------------------ |
| 0x70 | uint32\_t | magic           | 0xA5A5A5A5 | 结构魔数，用于完整性判断             |
| 0x74 | int16\_t  | temp\_high\_x10 | 280        | 温度上限 ×10（280 = 28.0℃）    |
| 0x76 | int16\_t  | humi\_high      | 80         | 湿度上限（% RH，整数）            |
| 0x78 | uint16\_t | smoke\_adc\_thr | 1500       | 烟雾报警阈值（12 位 ADC 原始值）     |
| 0x7A | uint16\_t | light\_low      | 5          | 光照下限（低于开灯，lx）            |
| 0x7C | uint16\_t | light\_high     | 20         | 光照上限（高于关灯，滞回，lx）         |
| 0x7E | uint16\_t | reserved        | 0x0000     | 对齐                       |

**API**：

```c
typedef struct {
    uint32_t magic;
    int16_t  temp_high_x10;
    int16_t  humi_high;
    uint16_t smoke_adc_thr;
    uint16_t light_low;
    uint16_t light_high;
    uint16_t reserved;
} app_config_t;             // 共 16 字节（取消 CRC32 字段）

void            CFG_Init(void);                        // 从 EEPROM 读取，失败写默认值
app_config_t*   CFG_Get(void);                         // 返回只读指针（内部 g_config）
void            CFG_SetTempHigh(int16_t temp_x10);     // 更新并写回 EEPROM
void            CFG_SetHumiHigh(int16_t humi);
void            CFG_SetSmokeThr(uint16_t adc_thr);
void            CFG_SetLightRange(uint16_t low, uint16_t high);
uint8_t         CFG_Save(void);                        // 手动触发写回 EEPROM（返回 0=OK）
```

**策略**：

- SetXxx 函数只更新 RAM 副本；用户点击“保存设置”后才统一调用一次 `CFG_Save()`，减少 EEPROM 写入
- 启动时读 EEPROM，若 magic 不对 → 用默认值并立即写回 EEPROM（取消 CRC32 校验）
- 若读到旧版默认光照范围 50/200 lx，则只迁移为 5/20 lx并保存，其他用户配置保持不变
- 配置结构体与 OTA 元数据区（0x00\~0x6B）无重叠

***

### 4.5 报警引擎（alarm\_engine.c/h）— 业务核心

**三级优先级（高到低）**：

1. **烟雾报警**：MQ-2 ADC 原始值 > `smoke_adc_thr` → 最高优先级
2. **温湿度超限**：温度 > 上限 或 湿度 > 上限 → 次优先级
3. **光照判断**：lux < `light_low` → 开灯；lux > `light_high` → 关灯（滞回）

**联动决策表**（AlarmEngineTask 每 200ms 计算一次输出）：

| 条件                    | 红灯 PF9 | 蜂鸣器 PF8                  | 双风扇 PE5/PE6（同步） | 绿灯 PF10 |
| --------------------- | ------ | ------------------------ | ------------- | ------- |
| 烟雾 ADC > 阈值           | 2Hz 快闪 | 1Hz 滴答断续声（200ms开+800ms关） | 100%            | 按光照独立   |
| 温度 > 上限（无烟雾）          | 灭      | 不响                       | 80%（当前固定值） | 按光照独立   |
| 湿度 > 上限（无烟雾）          | 灭      | 不响                       | 80%（当前固定值） | 按光照独立   |
| 温+湿双超限                | 灭      | 不响                       | 80%（当前固定值） | 按光照独立   |
| 烟雾+温湿度同时超限            | 2Hz 快闪 | 1Hz 滴答                   | 100%（覆盖环境控制） | 按光照独立   |
| 光照 < light\_low       | 不影响    | 不影响                      | 不影响             | 亮       |
| 光照 > light\_high 且前态亮 | 不影响    | 不影响                      | 不影响             | 灭       |
| 全部正常                  | 灭      | 不响                       | 停(0%)           | 按光照     |

**关键设计原则**：

- **报警覆盖**：烟雾报警时，双风扇强制 100% 同步输出；温湿度超限当前固定 80%，后续可替换为 PI 环境量闭环；报警解除后恢复
- **独立性**：绿灯（照明）完全独立于报警链路，不被任何报警覆盖
- **去抖动**：所有阈值判断带 2s 连续满足才切换状态，避免抖动
- **声光策略**：只有烟雾触发声光，温湿度只做风扇降温/排湿，无报警疲劳

**API**：

```c
typedef struct {
    uint16_t smoke_adc;     // 当前烟雾 ADC 原始值
    int16_t  temp_x10;      // 温度 ×10
    int16_t  humi;          // 湿度 %
    uint16_t lux;           // 光照 lx
} alarm_input_t;

typedef struct {
    uint8_t red_led_on;     // 红灯状态
    uint8_t buzzer_on;      // 蜂鸣器状态
    uint8_t fan_pct;        // 双风扇同步占空比 0~100
    uint8_t green_led_on;   // 绿灯状态
    uint8_t alarm_level;    // 0=无 1=温湿度 2=烟雾
} alarm_output_t;

void            ALARM_Init(void);
void            ALARM_UpdateInput(const alarm_input_t *in);   // Smoke/DHT/Light 任务写入
alarm_output_t* ALARM_GetOutput(void);                        // AlarmEngine 计算结果
uint8_t         ALARM_GetAlarmLevel(void);                    // 查询当前报警等级
```

**内部状态机**：

```
IDLE → 温湿度超限(LEVEL1) → 同时烟雾(LEVEL2)
 ↑                       ↓
 └────────── 清除 ←──────┘
光照判断与上述独立，不受状态机影响。
```

***

### 4.6 风扇控制服务（fan\_controller.c/h）— 执行器调度

**职责**：接收报警引擎输出，统一调度双风扇 PWM，保证只有一个硬件写入点。

**调度优先级（高到低）**：

1. 烟雾报警强制 100%（来自 alarm\_engine）
2. 温度或湿度超限固定 80%
3. 正常状态停止（0%）

**下一阶段 PI 设计边界**：

- 当前两线风扇没有转速反馈，下一步控制的是温湿度偏差到风扇占空比，不是风扇转速闭环
- 温度、湿度分别算需求占空比，取较大值；输出限幅，并加入最低运行占空比、启动助推和积分抗饱和
- 烟雾报警 100% 始终拥有最高优先级；传感器无效或 RS485 超时进入安全退化
- 第一版只用 PI，不加 D，控制周期按温湿度传感器速度设置为 2~5 秒

**API**：

```c
void     FANCTRL_Init(void);
void     FANCTRL_UpdateFromAlarm(uint8_t percent);  // Alarm 引擎给出的双风扇同步目标
void     FANCTRL_Run(void);                         // 周期调度并写 PWM
uint8_t  FANCTRL_GetActualDuty(void);               // 查询已写入的共同占空比
```

***

## 五、FreeRTOS 任务划分

### 5.1 任务总览

| 任务名 | 优先级 | 当前创建栈参数 | 周期 | 职责 |
| --- | ---: | ---: | ---: | --- |
| **SHT30Task** | 4 | 256 words（约1KB） | 1s | USART3读取Modbus定点温湿度，更新x10数据、UI、报警和UART2队列 |
| **SmokeTask** | 4 | 128 words（约512B） | 500ms | MQ-2取5点中位数，预热后更新报警输入 |
| **LightTask** | 3 | 128 words（约512B） | 1s | 读取BH1750 lux并更新报警输入 |
| **AlarmEngineTask** | 5 | 256 words（约1KB） | 200ms | 决策、去抖、周期业务上报以及风扇/LED/蜂鸣器唯一最终写入 |
| **UART2_Comm_Task** | 3 | 256 words（约1KB） | 事件驱动 | 业务帧转换、DMA发送、ACK/重试、OTA通知和版本查询 |
| **StartDefaultTask（LVGL）** | osPriorityLow2 | 8192B | 主循环5ms，数据刷新500ms | LVGL刷新、设置页绑定、状态显示和APP看门狗存活证明 |

当前没有独立FanCtrlTask或PropertyReportTask；对应工作已经合并到AlarmEngineTask及UART2通讯任务，避免增加无用任务和多写者。

### 5.2 数据流（任务间通信）

```
SmokeTask(ADC) ──raw_smoke───┐
SHT30Task(Modbus) ──x10──────┤
LightTask    ──lux───────────┼──► alarm_input_t 全局结构体（写方使用短临界区）
                            │
                            ▼
                   AlarmEngineTask(200ms)
                            │
               ┌────────────┼────────────┐
               ▼            ▼            ▼
        alarm_output_t  fan_controller  LED/BEEP
               │            │
               ▼            ▼
        UART2物模型上报   FAN_PWM_SetDuty(PE5/PE6)
        (type 0x01~09)
```

**同步方式**：alarm\_input\_t 结构体用 FreeRTOS 互斥锁保护，或使用 `taskENTER_CRITICAL()` 临界区（结构体 < 32 字节，开销可忽略）。

***

## 六、UART2 物模型协议扩展

ESP32 与 STM32 之间的业务帧（0x55 开头，8 字节固定）新增以下 type：

### 6.1 STM32 → ESP32 上报（新增 0x06\~0x08）

| type     | 名称    | data\[5] 用法              | 说明                               |
| -------- | ----- | ------------------------ | -------------------------------- |
| 0x01     | 湿度    | \[0]=湿度%, 其余0            | 已有                               |
| 0x02     | 温度    | \[0]=温度℃, 其余0            | 已有                               |
| 0x03     | LED红灯 | \[0]=0/1, 其余0            | 已有                               |
| 0x04     | 蜂鸣器   | \[0]=0/1, 其余0            | 已有                               |
| 0x05     | 版本号   | \[0]=M \[1]=m \[2]=p     | 已有                               |
| **0x06** | 烟雾    | \[0]=ADC高字节 \[1]=低字节 其余0 | **新增** 12位原始值 = (\[0]<<8)\|\[1] → OneNET属性 `Smoke`  |
| **0x07** | 光照    | \[0]=lux高字节 \[1]=低字节 其余0 | **新增** 16位 lux = (\[0]<<8)\|\[1] → OneNET属性 `Lux` |
| **0x08** | 风扇状态 | \[0]=占空比0\~100, 其余0      | **新增** 双风扇同步单指令，0=停 → OneNET属性 `Fan` |

### 6.2 ESP32 → STM32 下发（含心跳）

| type | 名称     | data\[5] 用法           | 说明 |
| ---- | ------ | --------------------- | -- |
| **0x0A** | **心跳** | \[0]=0 离线 / 1 在线，其余0 | **新增** ESP32→STM32，5s 周期，STM32 15s 未收到判离线；**不上云** |
| 0x81 | 设置温度上限 | \[0]=℃上限              | 预留（延后） |
| 0x82 | 设置湿度上限 | \[0]=%上限              | 预留（延后） |
| 0x83 | 设置烟雾阈值 | \[0]=高字节 \[1]=低字节     | 预留（延后） |
| 0x84 | 设置光照范围 | \[0]=下限低字节 \[1]=上限低字节 | 预留（延后） |
| 0x85 | 手动风扇  | \[0]=0\~100%, 30s超时恢复 | 预留（延后） |

### 6.3 OneNET 云平台属性映射（ESP32 端上报）

STM32 发来的业务帧经 ESP32 解析后，以下字段按对应标识符上报 OneNET：

| UART type | OneNET 标识符 | 数据类型 | 说明 |
| --- | --- | --- | --- |
| 0x06 | `Smoke` | int32 | 烟雾 ADC 原始值 |
| 0x07 | `Lux` | int32 | 光照 lux |
| 0x08 | `Fan` | int32 | 双风扇同步占空比 |
| 0x01 | `hum` | int32 | 湿度（已有） |
| 0x02 | `temp` | int32 | 温度（已有） |
| 0x03 | `LED` | bool | 照明灯（已有） |
| 0x04 | `Alarm` | bool | 蜂鸣器（已有） |

### 6.4 ACK 帧（不变）

`55 7E result 00 00 00 00 BB`，result=00 成功，01 帧错，02 队列满。

***

## 七、LVGL UI 扩展（初版必备页面）

### 7.1 新增页面

**1. 主状态页（首页扩展）**
在现有温湿度显示基础上，新增：

- 烟雾浓度条（ADC 原始值 + 阈值比较）
- 光照 lux 数值
- 双风扇占空比进度条
- 当前报警等级图标（无/温湿度/烟雾，颜色区分）

**2. 阈值设置页**

- 温度上限：±1 步进按钮，范围 -10℃ \~ 60℃
- 湿度上限：±1 步进按钮，范围 20% \~ 100%
- 烟雾阈值：±10 步进按钮，范围 0 \~ 4095
- 光照范围：下限 ±10 / 上限 ±10
- 「保存」按钮 → 调用 CFG\_SetXxx() → 立即写 EEPROM

**3. 报警状态页（可与首页合并，不单独做）**

- 实时显示执行器状态：红灯/蜂鸣器/风扇1/风扇2/绿灯
- 历史报警记录（可选，存 SRAM 环形缓冲）

### 7.2 UI 更新策略

- 首页数据刷新：每 500ms 从全局结构体读最新值更新 UI
- 设置页：修改值时**不立即**写 EEPROM，「保存」按钮按一下才写一次
- 阈值修改后立即生效（下次 AlarmEngineTask 周期读取最新值）

***

## 八、实施阶段划分（推荐顺序）

| 阶段           | 内容                                      | 预计工作项                                                                         | 验收标准                                                         |
| ------------ | --------------------------------------- | ----------------------------------------------------------------------------- | ------------------------------------------------------------ |
| **S1 驱动层就绪** | BSP 驱动 + 硬件改线                           | 风扇改 PE5/PE6 跳线；MQ-2 接 PA1；BH1750 接 PB6/PB7；实现 mq2.c / bh1750.c / fan\_pwm.c | 每个驱动单独测试：串口能打印烟雾 ADC 数值、光照 lux 数值、风扇占空比可调（手摸能感受转速变化）         |
| **S2 配置管理**  | config\_manager + EEPROM 读写             | 实现 app\_config\_t、magic 魔数校验、默认值回退（**取消 CRC32**）；CFG\_Init/Save/Get/Set 系列函数                  | 上电读默认值 → 用 CFG\_Set 修改 → 断电重启 → 值保持不变                        |
| **S3 报警引擎**  | alarm\_engine + fan\_controller + 3个新任务 | 实现决策表、状态机、去抖、联动；SmokeTask/LightTask/AlarmEngineTask                           | 用手捂住 MQ-2（或调临时阈值触发）→ 红灯快闪 + 蜂鸣器滴答 + 双风扇100%；光照手挡住 → 绿灯亮      |
| **S4 物模型上报** | UART2 协议扩展                              | 0x06 烟雾 / 0x07 光照 / 0x08 风扇（双风扇同步单指令） / 0x0A 心跳；OneNET 属性 Smoke/Lux/Fan；发送队列 + ACK 机制                         | ESP32 侧 idf.py monitor 能看到 3 种新 type 帧，ACK 正常；OneNET 控制台见 7 属性刷新；STM32 心跳灯变化 |
| **S5 UI 扩展** | LVGL 阈值页 + 首页扩展                         | 新增 UI 控件；保存按钮调 CFG 接口                                                         | 触摸屏改阈值 → 保存 → 生效 → 断电重启不丢                                    |
| **S6 增强项**  | RS485 / PI / OneNET 下发 | RS485工业温湿度采集器替换DHT11已完成；正式版v2.0.0 OTA回归通过；PI与0x8X控制帧继续延后 | RS485正常通讯、UI显示及正式OTA版本闭环已由BOSS确认；未保存完整日志的故障场景继续按边界记录 |

***

## 九、关键注意事项与坑点提醒

### 9.1 硬件

1. **风扇必须改线**：PG14/PG15 → PE5/PE6，否则无法 PWM 调速，只能开关。改线后测一下 MOS 管导通压降是否正常。
2. **PWM 频率必须实测**：当前两线风扇在 10 kHz、80% 时只响不转，改为 100 Hz 后正常起转。该频率是当前硬件实测值，不是所有风扇的通用结论。
3. **MQ-2 需要充分预热**：本次冷机实测约 15 分 46 秒后才从 4095 降到稳定区间，当前代码采用 20 分钟预热屏蔽；预热期间只采集显示，不参与报警。
4. **BH1750 地址 0x23**：ADDR 引脚接 GND（已做），软件 I2C 地址写 0x46 移位前的 0x23 即可（与 AT24C02 的 0xA0 不同格式，注意统一）。
5. **I2C 总线独立**：PB6/PB7 只挂 BH1750，不与 EEPROM/触摸 的软件 I2C（PB8/PB9）冲突，不要复用同一条 SCL/SDA。
6. **所有信号必须共地**：STM32 GND、外部电源 GND、传感器 GND 和风扇驱动 GND 必须连在一起；不同来源的 VCC 不要直接并联。
7. **风扇会干扰 MQ-2**：风扇 80% 工作时偶发 4095，停扇后突跳消失；具体是电源、地弹、开关噪声还是 ADC 输入过压仍待测量，不能只靠软件滤波掩盖。
8. **BH1750 基本通讯已验证**：遮挡窗口可接近 0；不变光源下数值长期上升仍需做风扇停/转和供电噪声对照，不能继续简单归因于 I2C 不通。
9. **SHT30使用外接RS485模块**：PB10/PB11只接自动方向TTL-RS485模块的RXD/TXD并共地，不使用板载RS485电路，PB12 DE/RE不参与控制。

### 9.2 软件

1. **CRC32 已取消**：config\_manager 初版不做 CRC32 校验，仅以 magic 魔数（0xA5A5A5A5）判数据有效性，简化实现（若后续需增强可再加回）。
2. **任务栈**：BH1750 软件 I2C 有死循环等待，LightTask 栈建议 256 字节起步，不够再扩。
3. **ADC 采样中值滤波**：MQ-2 做 5 次连续采样取中位数，不做平均，抗偶然尖峰。
4. **报警去抖**：所有阈值判断至少连续 2\~3 秒满足才切换，避免传感器偶发值导致频繁切换。
5. **EEPROM 写入次数**：AT24C02 写次数约 100 万次，不要在循环里 CFG\_Save()，只在用户按保存按钮时写。
6. **双风扇合并为单指令 0x08**：STM32 发一帧 `0x08 [0]=占空比`，ESP32 解析后写双路 PWM；不再分 0x08/0x09 两帧，减少 UART 流量与 ESP32 解析逻辑复杂度。
7. **`vTaskSuspend`** **禁止**：沿用 OTA 协议改造的教训，任何新任务都不要用 vTaskSuspend/vTaskDelete 冻结别的任务（尤其 UART ISR）。用标志位 g\_ota\_uart\_busy 门控模式。
8. **温湿度精度分层**：STM32内部和LVGL使用x10定点数保留1位小数；UART2 type 0x01/0x02发送前四舍五入为整数，ESP32和OneNET协议本轮不变。
9. **故障数据不能继续控制风扇**：Modbus失败后清温湿度有效标志，UI显示`NC`；报警引擎按原约2秒去抖退出温湿度报警。

### 9.3 OTA 兼容性

1. **物模型扩展不影响 OTA**：0x55 业务帧与 0x66 OTA 帧帧头不同，ESP32 端 `uart_recive_buff` 已按帧头分流。
2. **APP 版本号**：功能扩展完成后记得更新 AT24C02 版本号（从 1.0.16 升到 1.1.0 或 1.2.0，改 main.c 中的版本宏），确保版本闭环正常。
3. **Bootloader 不改动**：A 区新增代码不影响 Bootloader 状态机、Flash 布局、W25Q128 备份、IWDG。Bootloader 代码保持不动。

***

## 十、后续可选扩展（S6 加分项）

| 项目                        | 说明                                                  | 面试加分            |
| ------------------------- | --------------------------------------------------- | --------------- |
| 温湿度 PI 闭环                 | 完成RS485故障和全链路回归后，用PI控制环境偏差到风扇占空比；带限幅、最低运行占空比、启动助推和抗饱和 | ⭐⭐⭐⭐（控制算法） |
| RS485 Modbus RTU 替换 DHT11 | **已完成正式接入**：USART3（PB10/PB11）外接自动方向TTL-RS485模块，驱动来自已实机验证的STM32F103测试工程 | ⭐⭐⭐⭐⭐（工业通信）     |
| OneNET 平台下发阈值             | ESP32 端扩展 `0x8X` 控制帧解析，云平台远程调参                      | ⭐⭐⭐（云-端闭环）      |
| 报警历史记录                    | SRAM 环形缓冲存最近 50 条报警，LVGL 报警页可回看                     | ⭐⭐              |
| ESP32-S3 自身 OTA           | 用 OTA Manager 模式做 ESP32 自己的 A/B 分区升级                | ⭐⭐⭐⭐（双 MCU OTA） |

***

## 附录 A：文件与代码位置索引

| 文件                   | 路径                                                                                                                         | 变更                              |
| -------------------- | -------------------------------------------------------------------------------------------------------------------------- | ------------------------------- |
| 引脚文档                 | [ESP32S3\_STM32F407\_引脚清单与通信协议.txt](ESP32S3_STM32F407_引脚清单与通信协议.txt) | 第四节风扇/新增扩展；第七节业务协议              |
| MQ-2 头/源文件           | `STM32-OTA/A区智能哨兵环境监测系统/Drivers/Module_driver/inc/mq2.h`、`src/mq2.c` | 新建 |
| BH1750 头/源文件         | `STM32-OTA/A区智能哨兵环境监测系统/Drivers/Module_driver/inc/bh1750.h`、`src/bh1750.c` | 新建 |
| 风扇 PWM 头/源文件       | `STM32-OTA/A区智能哨兵环境监测系统/Drivers/Module_driver/inc/fan_pwm.h`、`src/fan_pwm.c` | 新建，原 FAN.h 保留兼容 |
| 三个服务模块              | `STM32-OTA/A区智能哨兵环境监测系统/Middlewares/Services/` | 新建配置、报警和风扇调度服务 |
| APP\_Tasks.c            | `STM32-OTA/A区智能哨兵环境监测系统/Core/Src/APP_Tasks.c` | 新增采集任务、报警任务和 UI 回调 |
| UART2 通讯               | `STM32-OTA/A区智能哨兵环境监测系统/Core/Src/uart2_comm.c` | 扩展 4 种业务帧 |
| LVGL 自定义逻辑          | `STM32-OTA/A区智能哨兵环境监测系统/LVGL/myGUI/custom/custom.c` | 阈值设置、状态文字和首页数据扩展 |
| Keil 工程文件            | `STM32-OTA/A区智能哨兵环境监测系统/MDK-ARM/LVGL_RTOS_DOME5.uvprojx` | 注册新增驱动、服务和 HAL ADC 文件 |
| 方案文档                 | [A区APP功能扩展方案.md](A区APP功能扩展方案.md) | 本文档 |

***

*本方案与* *`ESP32S3_STM32F407_引脚清单与通信协议.txt`* *第七节保持同步，如有改动需同时更新两处。*
