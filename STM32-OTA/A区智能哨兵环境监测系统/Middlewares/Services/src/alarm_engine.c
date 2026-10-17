#include "alarm_engine.h"
#include "config_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* 去抖周期数：AlarmEngineTask 每 200ms 调一次，10 次 = 2 秒确认 */
#define ALARM_DEBOUNCE_CYCLES       10U
/* 烟雾报警时红灯闪烁半周期，250ms 亮 + 250ms 灭 = 2Hz */
#define ALARM_RED_HALF_PERIOD_MS    250UL
/* 烟雾报警时蜂鸣器周期 */
#define ALARM_BUZZER_PERIOD_MS      1000UL
/* 蜂鸣器每个周期内的鸣叫时长 */
#define ALARM_BUZZER_ON_MS          200UL

static alarm_input_t g_alarm_input;         /**< 报警引擎输入快照，各传感器任务写入 */
static alarm_output_t g_alarm_output;       /**< 报警引擎输出快照，AlarmEngineTask 计算并发布 */
/* 以下为去抖后的稳定状态标志，1=条件持续满足 */
static uint8_t s_smoke_active = 0U;         /**< 烟雾报警去抖后状态 */
static uint8_t s_temp_active = 0U;          /**< 温度超限去抖后状态 */
static uint8_t s_humi_active = 0U;          /**< 湿度超限去抖后状态 */
static uint8_t s_light_on = 0U;             /**< 照明开关滞回后状态 */
/* 以下为各条件的连续计数器，达到 ALARM_DEBOUNCE_CYCLES 才翻转状态 */
static uint8_t s_smoke_on_count = 0U;       /**< 烟雾连续超阈值计数 */
static uint8_t s_smoke_off_count = 0U;      /**< 烟雾连续正常计数 */
static uint8_t s_temp_on_count = 0U;        /**< 温度连续超限计数 */
static uint8_t s_temp_off_count = 0U;       /**< 温度连续正常计数 */
static uint8_t s_humi_on_count = 0U;        /**< 湿度连续超限计数 */
static uint8_t s_humi_off_count = 0U;       /**< 湿度连续正常计数 */
static uint8_t s_light_low_count = 0U;      /**< 光照连续低于下限计数 */
static uint8_t s_light_high_count = 0U;     /**< 光照连续高于上限计数 */

static void ALARM_UpdateDebouncedState(uint8_t condition,
                                       uint8_t *state,
                                       uint8_t *on_count,
                                       uint8_t *off_count);
static uint8_t ALARM_IncrementSaturated(uint8_t value);

/**
 * @brief 对连续真假条件执行双向 2 秒去抖。
 * @param condition 当前原始条件。
 * @param state 去抖后的状态指针。
 * @param on_count 连续满足计数器。
 * @param off_count 连续不满足计数器。
 * @return 无。
 * @note 以 AlarmEngineTask 200 ms 周期计，10 次等于约 2 秒。
 */
static void ALARM_UpdateDebouncedState(uint8_t condition,
                                       uint8_t *state,
                                       uint8_t *on_count,
                                       uint8_t *off_count)
{
    if (condition != 0U)
    {
        /* 条件成立：on 计数器递增并饱和，同时清零 off 计数器（任何一次翻转都重置对侧计数） */
        *on_count = ALARM_IncrementSaturated(*on_count);
        *off_count = 0U;
        if (*on_count >= ALARM_DEBOUNCE_CYCLES)
        {
            *state = 1U;    /* 连续 N 次成立 → 去抖后状态置为"激活" */
        }
    }
    else
    {
        /* 条件不成立：off 计数器递增并饱和，同时清零 on 计数器 */
        *off_count = ALARM_IncrementSaturated(*off_count);
        *on_count = 0U;
        if (*off_count >= ALARM_DEBOUNCE_CYCLES)
        {
            *state = 0U;    /* 连续 N 次不成立 → 去抖后状态置为"解除" */
        }
    }
}

/**
 * @brief 递增并限制去抖计数器。
 * @param value 当前计数值。
 * @return 递增后的饱和值。
 * @note 上限固定为 ALARM_DEBOUNCE_CYCLES，防止长时间运行溢出。
 */
static uint8_t ALARM_IncrementSaturated(uint8_t value)
{
    return (value < ALARM_DEBOUNCE_CYCLES) ? (uint8_t)(value + 1U) : value;  /* 到上限后停止递增，防止长时间运行溢出 */
}

/**
 * @brief 初始化报警输入、输出和去抖状态。
 * @param 无。
 * @return 无。
 * @note 不直接访问 GPIO 或 PWM。
 */
void ALARM_Init(void)
{
    memset(&g_alarm_input, 0, sizeof(g_alarm_input));
    memset(&g_alarm_output, 0, sizeof(g_alarm_output));
    s_smoke_active = 0U;
    s_temp_active = 0U;
    s_humi_active = 0U;
    s_light_on = 0U;
    s_smoke_on_count = 0U;
    s_smoke_off_count = 0U;
    s_temp_on_count = 0U;
    s_temp_off_count = 0U;
    s_humi_on_count = 0U;
    s_humi_off_count = 0U;
    s_light_low_count = 0U;
    s_light_high_count = 0U;
}

/**
 * @brief 一次性更新完整报警输入。
 * @param input 输入结构体有效指针。
 * @return 无。
 * @note 复制过程受 FreeRTOS 临界区保护。
 */
void ALARM_UpdateInput(const alarm_input_t *input)
{
    if (input == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    g_alarm_input = *input;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新温湿度输入。
 * @param temp_x10 温度乘 10。
 * @param humi 湿度百分数。
 * @param valid 1表示本次温湿度有效，0表示传感器当前不可用。
 * @return 无。
 * @note 无效时不覆盖最后一次数值，只清有效标志，避免旧数据继续控制风扇。
 */
void ALARM_UpdateTempHumi(int16_t temp_x10, int16_t humi, uint8_t valid)
{
    taskENTER_CRITICAL();
    if (valid != 0U)
    {
        g_alarm_input.temp_x10 = temp_x10;
        g_alarm_input.humi = humi;
    }
    g_alarm_input.temp_humi_valid = (valid != 0U) ? 1U : 0U;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新烟雾 ADC 输入。
 * @param smoke_adc 12 位 ADC 原始值。
 * @return 无。
 * @note 只修改烟雾字段。
 */
void ALARM_UpdateSmoke(uint16_t smoke_adc)
{
    taskENTER_CRITICAL();
    g_alarm_input.smoke_adc = smoke_adc;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新光照输入。
 * @param lux 当前照度整数。
 * @return 无。
 * @note 只修改光照字段。
 */
void ALARM_UpdateLux(uint16_t lux)
{
    taskENTER_CRITICAL();
    g_alarm_input.lux = lux;
    taskEXIT_CRITICAL();
}

/**
 * @brief 计算最终报警与执行器输出。
 * @param now_ms 当前毫秒时基。
 * @param illumination_enabled 照明自动控制总开关。
 * @param manual_red_on 手动红灯状态。
 * @param manual_buzzer_on 手动蜂鸣器状态。
 * @return 无。
 * @note 烟雾最高优先；温湿度只驱动 80% 风扇；光照滞回独立计算。
 */
void ALARM_Run(uint32_t now_ms,
               uint8_t illumination_enabled,
               uint8_t manual_red_on,
               uint8_t manual_buzzer_on)
{
    alarm_input_t input;
    app_config_t config;
    alarm_output_t output;
    uint8_t smoke_condition;
    uint8_t temp_condition;
    uint8_t humi_condition;
    uint8_t environment_active;

    taskENTER_CRITICAL();
    input = g_alarm_input;
    taskEXIT_CRITICAL();
    CFG_GetSnapshot(&config);   /* 读取当前用户配置阈值快照 */

    /* 计算原始条件：直接与阈值比较，尚未去抖 */
    smoke_condition = (input.smoke_adc > config.smoke_adc_thr) ? 1U : 0U;
    temp_condition = ((input.temp_humi_valid != 0U) &&
                      (input.temp_x10 > config.temp_high_x10)) ? 1U : 0U;
    humi_condition = ((input.temp_humi_valid != 0U) &&
                      (input.humi > config.humi_high)) ? 1U : 0U;
    /* 分别对三个条件执行双向去抖 */
    ALARM_UpdateDebouncedState(smoke_condition,
                               &s_smoke_active,
                               &s_smoke_on_count,
                               &s_smoke_off_count);
    ALARM_UpdateDebouncedState(temp_condition,
                               &s_temp_active,
                               &s_temp_on_count,
                               &s_temp_off_count);
    ALARM_UpdateDebouncedState(humi_condition,
                               &s_humi_active,
                               &s_humi_on_count,
                               &s_humi_off_count);
    /* 环境告警 = 温度超限 OR 湿度超限（烟雾独立处理，优先级更高） */
    environment_active = ((s_temp_active != 0U) || (s_humi_active != 0U)) ? 1U : 0U;

    /* 光照滞回控制：用 light_low/light_high 两个阈值避免临界抖动 */
    if (input.lux < config.light_low)
    {
        s_light_low_count = ALARM_IncrementSaturated(s_light_low_count);
        s_light_high_count = 0U;
        if (s_light_low_count >= ALARM_DEBOUNCE_CYCLES)
        {
            s_light_on = 1U;    /* 连续低于下限 → 开照明 */
        }
    }
    else if (input.lux > config.light_high)
    {
        s_light_high_count = ALARM_IncrementSaturated(s_light_high_count);
        s_light_low_count = 0U;
        if (s_light_high_count >= ALARM_DEBOUNCE_CYCLES)
        {
            s_light_on = 0U;    /* 连续高于上限 → 关照明 */
        }
    }
    else
    {
        /* 光照处于滞回区间内，保持当前状态 */
        s_light_low_count = 0U;
        s_light_high_count = 0U;
    }

    /* 清零输出后按优先级填充 */
    memset(&output, 0, sizeof(output));
    output.green_led_on = ((illumination_enabled != 0U) && (s_light_on != 0U)) ? 1U : 0U;

    if (s_smoke_active != 0U)
    {
        /* === 烟雾报警（最高优先级）=== */
        output.alarm_level = 2U;
        output.fan_pct = 100U;                          /* 风扇全速排烟 */
        output.red_led_on = (((now_ms / ALARM_RED_HALF_PERIOD_MS) & 0x01UL) == 0UL) ? 1U : 0U;  /* 红灯 2Hz 闪烁 */
        output.buzzer_on = ((now_ms % ALARM_BUZZER_PERIOD_MS) < ALARM_BUZZER_ON_MS) ? 1U : 0U; /* 蜂鸣器 1s 周期响 200ms */
    }
    else
    {
        /* === 无烟雾：处理温湿度环境告警和手动控制 === */
        output.alarm_level = (environment_active != 0U) ? 1U : 0U;
        output.fan_pct = (environment_active != 0U) ? 80U : 0U;   /* 温湿度告警时风扇 80% 转速 */
        output.red_led_on = (manual_red_on != 0U) ? 1U : 0U;     /* 红灯由手动开关控制 */
        output.buzzer_on = (manual_buzzer_on != 0U) ? 1U : 0U;   /* 蜂鸣器由手动开关控制 */
    }

    taskENTER_CRITICAL();
    g_alarm_output = output;
    taskEXIT_CRITICAL();
}

/**
 * @brief 获取报警引擎当前输出指针。
 * @param 无。
 * @return 内部输出对象地址。
 * @note AlarmEngineTask 内部使用。
 */
const alarm_output_t *ALARM_GetOutput(void)
{
    return &g_alarm_output;
}

/**
 * @brief 原子复制报警输出快照。
 * @param output 接收输出快照的有效指针。
 * @return 无。
 * @note 复制时间固定且很短。
 */
void ALARM_GetOutputSnapshot(alarm_output_t *output)
{
    if (output == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    *output = g_alarm_output;
    taskEXIT_CRITICAL();
}

/**
 * @brief 获取当前报警等级。
 * @param 无。
 * @return 0=正常，1=温湿度超限，2=烟雾报警。
 * @note 读取单字节状态为原子操作。
 */
uint8_t ALARM_GetAlarmLevel(void)
{
    return g_alarm_output.alarm_level;
}
