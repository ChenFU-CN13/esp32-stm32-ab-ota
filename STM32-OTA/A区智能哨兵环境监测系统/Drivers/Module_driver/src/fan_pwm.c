#include "fan_pwm.h"

#define FAN_PWM_TARGET_COUNTER_HZ    10000UL
#define FAN_PWM_PERIOD_COUNTS        100UL

static TIM_HandleTypeDef s_htim9;
static uint8_t s_duty[2] = {0U, 0U};
static uint8_t s_ready = 0U;

/**
 * @brief 计算当前 TIM9 输入时钟。
 * @param 无。
 * @return TIM9 输入时钟频率，单位 Hz。
 * @note STM32F407 的 APB2 预分频不为 1 时，定时器时钟为 PCLK2 的 2 倍。
 */
static uint32_t FAN_PWM_GetTimerClockHz(void)
{
    RCC_ClkInitTypeDef clock_config;
    uint32_t flash_latency;
    uint32_t timer_clock_hz;

    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);
    timer_clock_hz = HAL_RCC_GetPCLK2Freq();
    if (clock_config.APB2CLKDivider != RCC_HCLK_DIV1)
    {
        timer_clock_hz *= 2UL;
    }

    return timer_clock_hz;
}

/**
 * @brief 初始化 TIM9 双通道风扇 PWM。
 * @param 无。
 * @return 无。
 * @note 计数频率为 10 kHz、ARR=99，因此 PWM 频率为 100 Hz。
 */
void FAN_PWM_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    TIM_OC_InitTypeDef pwm_config = {0};
    uint32_t timer_clock_hz;
    uint32_t prescaler_div;

    s_ready = 0U;
    s_duty[0] = 0U;
    s_duty[1] = 0U;

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_TIM9_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF3_TIM9;
    HAL_GPIO_Init(GPIOE, &gpio_init);

    timer_clock_hz = FAN_PWM_GetTimerClockHz();
    prescaler_div = timer_clock_hz / FAN_PWM_TARGET_COUNTER_HZ;
    if ((prescaler_div == 0UL) || (prescaler_div > 65536UL))
    {
        return;
    }

    s_htim9.Instance = TIM9;
    s_htim9.Init.Prescaler = prescaler_div - 1UL;
    s_htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_htim9.Init.Period = FAN_PWM_PERIOD_COUNTS - 1UL;
    s_htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_htim9.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&s_htim9) != HAL_OK)
    {
        return;
    }

    pwm_config.OCMode = TIM_OCMODE_PWM1;
    pwm_config.Pulse = 0UL;
    pwm_config.OCPolarity = TIM_OCPOLARITY_HIGH;
    pwm_config.OCFastMode = TIM_OCFAST_DISABLE;
    if ((HAL_TIM_PWM_ConfigChannel(&s_htim9, &pwm_config, TIM_CHANNEL_1) != HAL_OK) ||
        (HAL_TIM_PWM_ConfigChannel(&s_htim9, &pwm_config, TIM_CHANNEL_2) != HAL_OK))
    {
        return;
    }

    if ((HAL_TIM_PWM_Start(&s_htim9, TIM_CHANNEL_1) != HAL_OK) ||
        (HAL_TIM_PWM_Start(&s_htim9, TIM_CHANNEL_2) != HAL_OK))
    {
        return;
    }

    __HAL_TIM_SET_COMPARE(&s_htim9, TIM_CHANNEL_1, 0UL);
    __HAL_TIM_SET_COMPARE(&s_htim9, TIM_CHANNEL_2, 0UL);
    s_ready = 1U;
}

/**
 * @brief 设置指定风扇占空比。
 * @param fan_id 风扇编号：1 对应 TIM9_CH1，2 对应 TIM9_CH2。
 * @param percent 目标占空比 0~100%。
 * @return 无。
 * @note 非法编号或驱动未就绪时不写硬件。
 */
void FAN_PWM_SetDuty(uint8_t fan_id, uint8_t percent)
{
    uint32_t channel;
    uint8_t index;

    if ((s_ready == 0U) || (fan_id < 1U) || (fan_id > 2U))
    {
        return;
    }

    if (percent > 100U)
    {
        percent = 100U;
    }

    index = (uint8_t)(fan_id - 1U);
    channel = (fan_id == 1U) ? TIM_CHANNEL_1 : TIM_CHANNEL_2;
    __HAL_TIM_SET_COMPARE(&s_htim9, channel, percent);
    s_duty[index] = percent;
}

/**
 * @brief 获取指定风扇最近一次设置的占空比。
 * @param fan_id 风扇编号：1 或 2。
 * @return 0~100；编号无效时返回 0。
 * @note 返回软件缓存值。
 */
uint8_t FAN_PWM_GetDuty(uint8_t fan_id)
{
    if ((fan_id < 1U) || (fan_id > 2U))
    {
        return 0U;
    }

    return s_duty[fan_id - 1U];
}

/**
 * @brief 停止指定风扇。
 * @param fan_id 风扇编号：1 或 2。
 * @return 无。
 * @note 将对应 CCR 写为 0。
 */
void FAN_PWM_Stop(uint8_t fan_id)
{
    FAN_PWM_SetDuty(fan_id, 0U);
}

/**
 * @brief 指定风扇全速运行。
 * @param fan_id 风扇编号：1 或 2。
 * @return 无。
 * @note 将对应 CCR 写为 100，输出保持高电平。
 */
void FAN_PWM_StartMax(uint8_t fan_id)
{
    FAN_PWM_SetDuty(fan_id, 100U);
}

/**
 * @brief 查询 PWM 驱动是否完成初始化。
 * @param 无。
 * @return 1=可用，0=不可用。
 * @note 只读软件状态。
 */
uint8_t FAN_PWM_IsReady(void)
{
    return s_ready;
}
