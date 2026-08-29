#include "config_manager.h"
#include "FreeRTOS.h"
#include "task.h"
#include "./BSP/24CXX/24cxx.h"
#include <stdio.h>
#include <string.h>

typedef char app_config_size_must_be_16_bytes[(sizeof(app_config_t) == 16U) ? 1 : -1];

static const app_config_t s_default_config = {
    APP_CONFIG_MAGIC,
    280,
    80,
    1500U,
    5U,
    20U,
    0U
};

static app_config_t g_config;

/**
 * @brief 从 AT24C02 读取配置，必要时写入默认值。
 * @param 无。
 * @return 无。
 * @note 初版按约定只校验 magic，不实现 CRC32。
 */
void CFG_Init(void)
{
    uint8_t save_result;

    at24cxx_init();
    at24cxx_read(APP_CONFIG_EEPROM_ADDRESS,
                 (uint8_t *)&g_config,
                 (uint16_t)sizeof(g_config));

    if (g_config.magic != APP_CONFIG_MAGIC)
    {
        g_config = s_default_config;
        save_result = CFG_Save();
        printf("[CFG] invalid magic, default save=%s\r\n",
               (save_result == 0U) ? "OK" : "FAIL");
    }
    else
    {
        /* 只迁移旧版默认光照阈值，不改动用户设置过的其他参数。 */
        if ((g_config.light_low == 50U) && (g_config.light_high == 200U))
        {
            g_config.light_low = 5U;
            g_config.light_high = 20U;
            save_result = CFG_Save();
            printf("[CFG] migrate Lux=50/200 -> 5/20 save=%s\r\n",
                   (save_result == 0U) ? "OK" : "FAIL");
        }

        printf("[CFG] load OK T=%d.%d H=%d Smoke=%u Lux=%u/%u\r\n",
               (int)(g_config.temp_high_x10 / 10),
               (int)((g_config.temp_high_x10 >= 0) ?
                     (g_config.temp_high_x10 % 10) :
                     -(g_config.temp_high_x10 % 10)),
               (int)g_config.humi_high,
               (unsigned int)g_config.smoke_adc_thr,
               (unsigned int)g_config.light_low,
               (unsigned int)g_config.light_high);
    }
}

/**
 * @brief 获取配置对象只读指针。
 * @param 无。
 * @return 内部配置对象地址。
 * @note 调用方不能通过强制转换修改该对象。
 */
const app_config_t *CFG_Get(void)
{
    return &g_config;
}

/**
 * @brief 原子复制一份当前配置。
 * @param out_config 接收配置快照的有效指针。
 * @return 无。
 * @note 复制期间不允许任务切换造成跨字段不一致。
 */
void CFG_GetSnapshot(app_config_t *out_config)
{
    if (out_config == NULL)
    {
        return;
    }

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        /* CFG_Init 会在调度器启动前保存默认值，此时没有并发任务。 */
        *out_config = g_config;
    }
    else
    {
        taskENTER_CRITICAL();
        *out_config = g_config;
        taskEXIT_CRITICAL();
    }
}

/**
 * @brief 更新 RAM 中的温度上限。
 * @param temp_x10 温度上限乘 10。
 * @return 无。
 * @note UI 已限制为 -10~60 摄氏度，此处再做防御性限幅。
 */
void CFG_SetTempHigh(int16_t temp_x10)
{
    if (temp_x10 < -100)
    {
        temp_x10 = -100;
    }
    else if (temp_x10 > 600)
    {
        temp_x10 = 600;
    }

    taskENTER_CRITICAL();
    g_config.temp_high_x10 = temp_x10;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新 RAM 中的湿度上限。
 * @param humi 湿度上限百分数。
 * @return 无。
 * @note UI 范围为 20~100%。
 */
void CFG_SetHumiHigh(int16_t humi)
{
    if (humi < 20)
    {
        humi = 20;
    }
    else if (humi > 100)
    {
        humi = 100;
    }

    taskENTER_CRITICAL();
    g_config.humi_high = humi;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新 RAM 中的烟雾 ADC 阈值。
 * @param adc_thr 12 位 ADC 阈值。
 * @return 无。
 * @note 大于 4095 时按 4095 处理。
 */
void CFG_SetSmokeThr(uint16_t adc_thr)
{
    if (adc_thr > 4095U)
    {
        adc_thr = 4095U;
    }

    taskENTER_CRITICAL();
    g_config.smoke_adc_thr = adc_thr;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新 RAM 中的光照滞回范围。
 * @param low 低阈值。
 * @param high 高阈值。
 * @return 无。
 * @note high 不大于 low 时忽略修改，避免滞回区间反转。
 */
void CFG_SetLightRange(uint16_t low, uint16_t high)
{
    if (high <= low)
    {
        return;
    }

    taskENTER_CRITICAL();
    g_config.light_low = low;
    g_config.light_high = high;
    taskEXIT_CRITICAL();
}

/**
 * @brief 恢复默认配置并立即保存。
 * @param 无。
 * @return 0=保存成功，1=读回校验失败。
 * @note 只进行一次 CFG_Save。
 */
uint8_t CFG_SetDefault(void)
{
    taskENTER_CRITICAL();
    g_config = s_default_config;
    taskEXIT_CRITICAL();
    return CFG_Save();
}

/**
 * @brief 将 RAM 配置保存到 AT24C02 并读回校验。
 * @param 无。
 * @return 0=读回一致，1=读回不一致。
 * @note 写入和读回在调用任务上下文执行，禁止在 ISR 或采集循环中调用。
 */
uint8_t CFG_Save(void)
{
    app_config_t config_snapshot;
    uint8_t readback[sizeof(app_config_t)];

    CFG_GetSnapshot(&config_snapshot);
    at24cxx_write(APP_CONFIG_EEPROM_ADDRESS,
                  (uint8_t *)&config_snapshot,
                  (uint16_t)sizeof(config_snapshot));
    at24cxx_read(APP_CONFIG_EEPROM_ADDRESS,
                 readback,
                 (uint16_t)sizeof(readback));

    return (memcmp(readback, &config_snapshot, sizeof(config_snapshot)) == 0) ? 0U : 1U;
}
