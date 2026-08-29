#ifndef __FAN_PWM_H
#define __FAN_PWM_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 TIM9 双通道风扇 PWM。
 * @param 无。
 * @return 无。
 * @note PE5=TIM9_CH1、PE6=TIM9_CH2；频率目标为 10 kHz，初始化后默认 0%。
 */
void FAN_PWM_Init(void);

/**
 * @brief 设置指定风扇占空比。
 * @param fan_id 风扇编号：1 对应 PE5，2 对应 PE6。
 * @param percent 占空比百分数，超过 100 时按 100 处理。
 * @return 无。
 * @note 本接口非线程安全，由 fan_controller 单点调用。
 */
void FAN_PWM_SetDuty(uint8_t fan_id, uint8_t percent);

/**
 * @brief 获取指定风扇最近一次设置的占空比。
 * @param fan_id 风扇编号：1 或 2。
 * @return 0~100；编号无效时返回 0。
 * @note 返回软件缓存值，不读取 TIM9 CCR 寄存器。
 */
uint8_t FAN_PWM_GetDuty(uint8_t fan_id);

/**
 * @brief 停止指定风扇。
 * @param fan_id 风扇编号：1 或 2。
 * @return 无。
 * @note 等价于 FAN_PWM_SetDuty(fan_id, 0)。
 */
void FAN_PWM_Stop(uint8_t fan_id);

/**
 * @brief 指定风扇全速运行。
 * @param fan_id 风扇编号：1 或 2。
 * @return 无。
 * @note 等价于 FAN_PWM_SetDuty(fan_id, 100)。
 */
void FAN_PWM_StartMax(uint8_t fan_id);

/**
 * @brief 查询 PWM 驱动是否完成初始化。
 * @param 无。
 * @return 1=可用，0=初始化失败或尚未初始化。
 * @note 供上层输出简洁故障日志，不在驱动内部打印。
 */
uint8_t FAN_PWM_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif
