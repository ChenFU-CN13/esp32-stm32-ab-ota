#ifndef __MQ2_H
#define __MQ2_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQ2_INVALID_RAW    0xFFFFU

/**
 * @brief 初始化 MQ-2 模拟量采集通道。
 * @param 无。
 * @return 无。
 * @note 使用 PA1/ADC1_IN1，12 位单次软件触发模式。
 */
void MQ2_Init(void);

/**
 * @brief 读取一次 MQ-2 ADC 原始值。
 * @param 无。
 * @return 0~4095 为有效值；MQ2_INVALID_RAW 表示启动或转换超时。
 * @note 转换轮询上限为 5 ms，不会无限等待。
 */
uint16_t MQ2_ReadRaw(void);

/**
 * @brief 多次采样并取中位数。
 * @param samples 采样数，合法范围 1~9；任务使用 5 次。
 * @return 0~4095 为中位数；参数错误或任一次采样失败返回 MQ2_INVALID_RAW。
 * @note 使用固定 9 元素栈数组，不进行动态分配。
 */
uint16_t MQ2_ReadFiltered(uint8_t samples);

/**
 * @brief 查询 ADC 驱动是否完成初始化。
 * @param 无。
 * @return 1=可用，0=初始化失败或尚未初始化。
 * @note 供任务输出初始化结果。
 */
uint8_t MQ2_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif
