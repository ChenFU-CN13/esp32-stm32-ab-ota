#include "mq2.h"

#define MQ2_ADC_TIMEOUT_MS       5UL
#define MQ2_MAX_FILTER_SAMPLES   9U

static ADC_HandleTypeDef s_hadc1;
static uint8_t s_ready = 0U;

/**
 * @brief 初始化 MQ-2 模拟量采集通道。
 * @param 无。
 * @return 无。
 * @note ADC 时钟为 PCLK2/4；当前 84 MHz PCLK2 下得到 21 MHz ADCCLK。
 */
void MQ2_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    ADC_ChannelConfTypeDef channel_config = {0};

    s_ready = 0U;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    gpio_init.Pin = GPIO_PIN_1;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio_init);

    s_hadc1.Instance = ADC1;
    s_hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    s_hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    s_hadc1.Init.ScanConvMode = DISABLE;
    s_hadc1.Init.ContinuousConvMode = DISABLE;
    s_hadc1.Init.DiscontinuousConvMode = DISABLE;
    s_hadc1.Init.NbrOfDiscConversion = 0U;
    s_hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    s_hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    s_hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    s_hadc1.Init.NbrOfConversion = 1U;
    s_hadc1.Init.DMAContinuousRequests = DISABLE;
    s_hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&s_hadc1) != HAL_OK)
    {
        return;
    }

    channel_config.Channel = ADC_CHANNEL_1;
    channel_config.Rank = 1U;
    channel_config.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    channel_config.Offset = 0U;
    if (HAL_ADC_ConfigChannel(&s_hadc1, &channel_config) != HAL_OK)
    {
        return;
    }

    s_ready = 1U;
}

/**
 * @brief 读取一次 MQ-2 ADC 原始值。
 * @param 无。
 * @return 0~4095 为有效值；MQ2_INVALID_RAW 表示失败。
 * @note 无论轮询成功与否都会停止本次 ADC 转换。
 */
uint16_t MQ2_ReadRaw(void)
{
    uint16_t raw;

    if (s_ready == 0U)
    {
        return MQ2_INVALID_RAW;
    }

    if (HAL_ADC_Start(&s_hadc1) != HAL_OK)
    {
        return MQ2_INVALID_RAW;
    }

    if (HAL_ADC_PollForConversion(&s_hadc1, MQ2_ADC_TIMEOUT_MS) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&s_hadc1);
        return MQ2_INVALID_RAW;
    }

    raw = (uint16_t)HAL_ADC_GetValue(&s_hadc1);
    (void)HAL_ADC_Stop(&s_hadc1);
    return raw;
}

/**
 * @brief 多次采样并取中位数。
 * @param samples 采样数，合法范围 1~9。
 * @return 0~4095 为中位数；失败返回 MQ2_INVALID_RAW。
 * @note 插入排序只处理实际采样元素，固定内存开销为 18 字节。
 */
uint16_t MQ2_ReadFiltered(uint8_t samples)
{
    uint16_t values[MQ2_MAX_FILTER_SAMPLES];
    uint16_t current;
    int16_t position;
    uint8_t index;

    if ((samples == 0U) || (samples > MQ2_MAX_FILTER_SAMPLES))
    {
        return MQ2_INVALID_RAW;
    }

    for (index = 0U; index < samples; index++)
    {
        current = MQ2_ReadRaw();
        if (current == MQ2_INVALID_RAW)
        {
            return MQ2_INVALID_RAW;
        }

        position = (int16_t)index - 1;
        while ((position >= 0) && (values[position] > current))
        {
            values[position + 1] = values[position];
            position--;
        }
        values[position + 1] = current;
    }

    return values[samples / 2U];
}

/**
 * @brief 查询 ADC 驱动是否完成初始化。
 * @param 无。
 * @return 1=可用，0=不可用。
 * @note 只读软件状态。
 */
uint8_t MQ2_IsReady(void)
{
    return s_ready;
}
