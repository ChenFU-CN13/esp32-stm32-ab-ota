#include "alarm_engine.h"
#include "config_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define ALARM_DEBOUNCE_CYCLES       10U
#define ALARM_RED_HALF_PERIOD_MS    250UL
#define ALARM_BUZZER_PERIOD_MS      1000UL
#define ALARM_BUZZER_ON_MS          200UL

static alarm_input_t g_alarm_input;
static alarm_output_t g_alarm_output;
static uint8_t s_smoke_active = 0U;
static uint8_t s_temp_active = 0U;
static uint8_t s_humi_active = 0U;
static uint8_t s_light_on = 0U;
static uint8_t s_smoke_on_count = 0U;
static uint8_t s_smoke_off_count = 0U;
static uint8_t s_temp_on_count = 0U;
static uint8_t s_temp_off_count = 0U;
static uint8_t s_humi_on_count = 0U;
static uint8_t s_humi_off_count = 0U;
static uint8_t s_light_low_count = 0U;
static uint8_t s_light_high_count = 0U;

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
        *on_count = ALARM_IncrementSaturated(*on_count);
        *off_count = 0U;
        if (*on_count >= ALARM_DEBOUNCE_CYCLES)
        {
            *state = 1U;
        }
    }
    else
    {
        *off_count = ALARM_IncrementSaturated(*off_count);
        *on_count = 0U;
        if (*off_count >= ALARM_DEBOUNCE_CYCLES)
        {
            *state = 0U;
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
    return (value < ALARM_DEBOUNCE_CYCLES) ? (uint8_t)(value + 1U) : value;
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
 * @return 无。
 * @note 只修改对应字段。
 */
void ALARM_UpdateDht(int16_t temp_x10, int16_t humi)
{
    taskENTER_CRITICAL();
    g_alarm_input.temp_x10 = temp_x10;
    g_alarm_input.humi = humi;
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
    CFG_GetSnapshot(&config);

    smoke_condition = (input.smoke_adc > config.smoke_adc_thr) ? 1U : 0U;
    temp_condition = (input.temp_x10 > config.temp_high_x10) ? 1U : 0U;
    humi_condition = (input.humi > config.humi_high) ? 1U : 0U;
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
    environment_active = ((s_temp_active != 0U) || (s_humi_active != 0U)) ? 1U : 0U;

    if (input.lux < config.light_low)
    {
        s_light_low_count = ALARM_IncrementSaturated(s_light_low_count);
        s_light_high_count = 0U;
        if (s_light_low_count >= ALARM_DEBOUNCE_CYCLES)
        {
            s_light_on = 1U;
        }
    }
    else if (input.lux > config.light_high)
    {
        s_light_high_count = ALARM_IncrementSaturated(s_light_high_count);
        s_light_low_count = 0U;
        if (s_light_high_count >= ALARM_DEBOUNCE_CYCLES)
        {
            s_light_on = 0U;
        }
    }
    else
    {
        s_light_low_count = 0U;
        s_light_high_count = 0U;
    }

    memset(&output, 0, sizeof(output));
    output.green_led_on = ((illumination_enabled != 0U) && (s_light_on != 0U)) ? 1U : 0U;
    if (s_smoke_active != 0U)
    {
        output.alarm_level = 2U;
        output.fan_pct = 100U;
        output.red_led_on = (((now_ms / ALARM_RED_HALF_PERIOD_MS) & 0x01UL) == 0UL) ? 1U : 0U;
        output.buzzer_on = ((now_ms % ALARM_BUZZER_PERIOD_MS) < ALARM_BUZZER_ON_MS) ? 1U : 0U;
    }
    else
    {
        output.alarm_level = (environment_active != 0U) ? 1U : 0U;
        output.fan_pct = (environment_active != 0U) ? 80U : 0U;
        output.red_led_on = (manual_red_on != 0U) ? 1U : 0U;
        output.buzzer_on = (manual_buzzer_on != 0U) ? 1U : 0U;
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
