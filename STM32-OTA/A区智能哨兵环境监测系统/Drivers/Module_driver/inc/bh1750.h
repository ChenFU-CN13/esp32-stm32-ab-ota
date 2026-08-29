#ifndef __BH1750_H
#define __BH1750_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 BH1750 软件 I2C 与连续高分辨率模式。
 * @param 无。
 * @return 无。
 * @note PB6=SCL、PB7=SDA、ADDR 接地，首次转换等待上限按 180 ms 处理。
 */
void BH1750_Init(void);

/**
 * @brief 读取当前环境照度。
 * @param out_lux 输出照度整数，单位 lx。
 * @return 0=成功，非 0=参数、总线或应答错误。
 * @note 默认 MTreg=69 时按 ROHM 公式 raw/1.2 换算，使用整数 raw*5/6。
 */
uint8_t BH1750_ReadLux(uint16_t *out_lux);

/**
 * @brief 查询 BH1750 是否完成初始化。
 * @param 无。
 * @return 1=可用，0=初始化失败或尚未初始化。
 * @note 供 LightTask 输出一次性初始化结果。
 */
uint8_t BH1750_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif
