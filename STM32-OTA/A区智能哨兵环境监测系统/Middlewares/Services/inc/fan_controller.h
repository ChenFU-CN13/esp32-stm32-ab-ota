#ifndef __FAN_CONTROLLER_H
#define __FAN_CONTROLLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化双风扇调度状态并强制输出 0%。
 * @param 无。
 * @return 无。
 * @note PWM 硬件应已由 FAN_PWM_Init 初始化。
 */
void FANCTRL_Init(void);

/**
 * @brief 更新报警引擎给出的双风扇同步目标值。
 * @param percent 目标占空比 0~100%。
 * @return 无。
 * @note 初版不接受 LVGL 手动占空比，screen_bar_fan 只显示最终输出。
 */
void FANCTRL_UpdateFromAlarm(uint8_t percent);

/**
 * @brief 将当前目标单点写入两路 PWM。
 * @param 无。
 * @return 无。
 * @note 仅在目标变化时写 TIM9，避免多个任务争用执行器。
 */
void FANCTRL_Run(void);

/**
 * @brief 获取双风扇当前实际占空比。
 * @param 无。
 * @return 0~100%。
 * @note 两路初版始终同步，返回共同值。
 */
uint8_t FANCTRL_GetActualDuty(void);

#ifdef __cplusplus
}
#endif

#endif
