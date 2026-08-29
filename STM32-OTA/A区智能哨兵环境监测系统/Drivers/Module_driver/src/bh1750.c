#include "bh1750.h"
#include "./SYSTEM/delay/delay.h"

#define BH1750_SCL_PIN              GPIO_PIN_6
#define BH1750_SDA_PIN              GPIO_PIN_7
#define BH1750_GPIO_PORT            GPIOB
#define BH1750_ADDRESS_7BIT         0x23U
#define BH1750_COMMAND_POWER_ON     0x01U
#define BH1750_COMMAND_CONT_H_RES   0x10U
#define BH1750_HALF_PERIOD_US       3U
#define BH1750_ACK_TIMEOUT_US       100U
#define BH1750_FIRST_MEASURE_MS     180U

static uint8_t s_ready = 0U;

static void BH1750_SclWrite(GPIO_PinState state);
static void BH1750_SdaWrite(GPIO_PinState state);
static uint8_t BH1750_SdaRead(void);
static void BH1750_I2C_Start(void);
static void BH1750_I2C_Stop(void);
static uint8_t BH1750_I2C_WriteByte(uint8_t data);
static uint8_t BH1750_I2C_ReadByte(uint8_t send_ack);
static void BH1750_I2C_Recover(void);
static uint8_t BH1750_WriteCommand(uint8_t command);

/**
 * @brief 设置软件 I2C 时钟线电平。
 * @param state GPIO_PIN_SET 表示释放总线，GPIO_PIN_RESET 表示拉低。
 * @return 无。
 * @note GPIO 使用开漏输出，写高不会主动驱动高电平。
 */
static void BH1750_SclWrite(GPIO_PinState state)
{
    HAL_GPIO_WritePin(BH1750_GPIO_PORT, BH1750_SCL_PIN, state);
}

/**
 * @brief 设置软件 I2C 数据线电平。
 * @param state GPIO_PIN_SET 表示释放总线，GPIO_PIN_RESET 表示拉低。
 * @return 无。
 * @note 读取前必须先释放 SDA。
 */
static void BH1750_SdaWrite(GPIO_PinState state)
{
    HAL_GPIO_WritePin(BH1750_GPIO_PORT, BH1750_SDA_PIN, state);
}

/**
 * @brief 读取软件 I2C 数据线。
 * @param 无。
 * @return 1=高电平，0=低电平。
 * @note 直接读取 GPIO IDR。
 */
static uint8_t BH1750_SdaRead(void)
{
    return (HAL_GPIO_ReadPin(BH1750_GPIO_PORT, BH1750_SDA_PIN) == GPIO_PIN_SET) ? 1U : 0U;
}

/**
 * @brief 产生软件 I2C 起始条件。
 * @param 无。
 * @return 无。
 * @note 总线空闲时 SDA 在 SCL 为高期间由高变低。
 */
static void BH1750_I2C_Start(void)
{
    BH1750_SdaWrite(GPIO_PIN_SET);
    BH1750_SclWrite(GPIO_PIN_SET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SdaWrite(GPIO_PIN_RESET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SclWrite(GPIO_PIN_RESET);
}

/**
 * @brief 产生软件 I2C 停止条件。
 * @param 无。
 * @return 无。
 * @note SDA 在 SCL 为高期间由低变高。
 */
static void BH1750_I2C_Stop(void)
{
    BH1750_SdaWrite(GPIO_PIN_RESET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SclWrite(GPIO_PIN_SET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SdaWrite(GPIO_PIN_SET);
    delay_us(BH1750_HALF_PERIOD_US);
}

/**
 * @brief 发送一个字节并等待从机 ACK。
 * @param data 待发送字节，高位先发。
 * @return 0=收到 ACK，1=ACK 超时。
 * @note ACK 等待被限制为 100 us，不会因设备缺失而死循环。
 */
static uint8_t BH1750_I2C_WriteByte(uint8_t data)
{
    uint16_t wait_us;
    uint8_t bit_index;

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        BH1750_SdaWrite(((data & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        delay_us(BH1750_HALF_PERIOD_US);
        BH1750_SclWrite(GPIO_PIN_SET);
        delay_us(BH1750_HALF_PERIOD_US);
        BH1750_SclWrite(GPIO_PIN_RESET);
        data <<= 1U;
    }

    BH1750_SdaWrite(GPIO_PIN_SET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SclWrite(GPIO_PIN_SET);
    for (wait_us = 0U; wait_us < BH1750_ACK_TIMEOUT_US; wait_us++)
    {
        if (BH1750_SdaRead() == 0U)
        {
            break;
        }
        delay_us(1U);
    }
    BH1750_SclWrite(GPIO_PIN_RESET);

    return (wait_us >= BH1750_ACK_TIMEOUT_US) ? 1U : 0U;
}

/**
 * @brief 从软件 I2C 读取一个字节。
 * @param send_ack 1=读完发送 ACK，0=读完发送 NACK。
 * @return 接收到的 8 位数据。
 * @note 高位先接收。
 */
static uint8_t BH1750_I2C_ReadByte(uint8_t send_ack)
{
    uint8_t data = 0U;
    uint8_t bit_index;

    BH1750_SdaWrite(GPIO_PIN_SET);
    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        data <<= 1U;
        BH1750_SclWrite(GPIO_PIN_SET);
        delay_us(BH1750_HALF_PERIOD_US);
        if (BH1750_SdaRead() != 0U)
        {
            data |= 1U;
        }
        BH1750_SclWrite(GPIO_PIN_RESET);
        delay_us(BH1750_HALF_PERIOD_US);
    }

    BH1750_SdaWrite((send_ack != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SclWrite(GPIO_PIN_SET);
    delay_us(BH1750_HALF_PERIOD_US);
    BH1750_SclWrite(GPIO_PIN_RESET);
    BH1750_SdaWrite(GPIO_PIN_SET);

    return data;
}

/**
 * @brief 尝试释放被从机占用的软件 I2C 总线。
 * @param 无。
 * @return 无。
 * @note 释放 SDA 后输出 9 个 SCL 脉冲，再产生 STOP；总耗时有界。
 */
static void BH1750_I2C_Recover(void)
{
    uint8_t pulse;

    BH1750_SdaWrite(GPIO_PIN_SET);
    for (pulse = 0U; pulse < 9U; pulse++)
    {
        BH1750_SclWrite(GPIO_PIN_RESET);
        delay_us(BH1750_HALF_PERIOD_US);
        BH1750_SclWrite(GPIO_PIN_SET);
        delay_us(BH1750_HALF_PERIOD_US);
    }
    BH1750_I2C_Stop();
}

/**
 * @brief 向 BH1750 写入单字节指令。
 * @param command BH1750 指令码。
 * @return 0=成功，非 0=地址或数据未应答。
 * @note 地址使用 7 位 0x23 左移后的写地址 0x46。
 */
static uint8_t BH1750_WriteCommand(uint8_t command)
{
    BH1750_I2C_Start();
    if (BH1750_I2C_WriteByte((uint8_t)(BH1750_ADDRESS_7BIT << 1U)) != 0U)
    {
        BH1750_I2C_Stop();
        return 1U;
    }
    if (BH1750_I2C_WriteByte(command) != 0U)
    {
        BH1750_I2C_Stop();
        return 2U;
    }
    BH1750_I2C_Stop();
    return 0U;
}

/**
 * @brief 初始化 BH1750 软件 I2C 与连续高分辨率模式。
 * @param 无。
 * @return 无。
 * @note 首次转换按数据手册最大值等待 180 ms；初始化阶段调用一次。
 */
void BH1750_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    s_ready = 0U;
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio_init.Pin = BH1750_SCL_PIN | BH1750_SDA_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BH1750_GPIO_PORT, &gpio_init);
    BH1750_SclWrite(GPIO_PIN_SET);
    BH1750_SdaWrite(GPIO_PIN_SET);
    BH1750_I2C_Recover();

    if ((BH1750_WriteCommand(BH1750_COMMAND_POWER_ON) != 0U) ||
        (BH1750_WriteCommand(BH1750_COMMAND_CONT_H_RES) != 0U))
    {
        return;
    }

    delay_ms(BH1750_FIRST_MEASURE_MS);
    s_ready = 1U;
}

/**
 * @brief 读取当前环境照度。
 * @param out_lux 输出照度整数，单位 lx。
 * @return 0=成功，非 0=参数、未初始化或地址未应答。
 * @note 默认 MTreg=69，换算为四舍五入的 raw*5/6，避免引入浮点运行库。
 */
uint8_t BH1750_ReadLux(uint16_t *out_lux)
{
    uint16_t raw;
    uint8_t high_byte;
    uint8_t low_byte;

    if ((out_lux == NULL) || (s_ready == 0U))
    {
        return 1U;
    }

    BH1750_I2C_Start();
    if (BH1750_I2C_WriteByte((uint8_t)((BH1750_ADDRESS_7BIT << 1U) | 0x01U)) != 0U)
    {
        BH1750_I2C_Stop();
        BH1750_I2C_Recover();
        return 2U;
    }

    high_byte = BH1750_I2C_ReadByte(1U);
    low_byte = BH1750_I2C_ReadByte(0U);
    BH1750_I2C_Stop();

    raw = (uint16_t)(((uint16_t)high_byte << 8U) | low_byte);
    *out_lux = (uint16_t)((((uint32_t)raw * 5UL) + 3UL) / 6UL);
    return 0U;
}

/**
 * @brief 查询 BH1750 是否完成初始化。
 * @param 无。
 * @return 1=可用，0=不可用。
 * @note 只读软件状态。
 */
uint8_t BH1750_IsReady(void)
{
    return s_ready;
}
