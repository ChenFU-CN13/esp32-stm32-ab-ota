#include "fan_controller.h"
#include "fan_pwm.h"

static uint8_t s_alarm_target_pct = 0U;
static uint8_t s_actual_pct = 0U;

/**
 * @brief 初始化双风扇调度状态并强制输出 0%。
 * @param 无。
 * @return 无。
 * @note 两路风扇同步输出。
 */
void FANCTRL_Init(void)
{
    s_alarm_target_pct = 0U;
    s_actual_pct = 0U;
    FAN_PWM_SetDuty(1U, 0U);
    FAN_PWM_SetDuty(2U, 0U);
}

/**
 * @brief 更新报警引擎给出的双风扇同步目标值。
 * @param percent 目标占空比 0~100%。
 * @return 无。
 * @note 超过 100 时按 100 处理。
 */
void FANCTRL_UpdateFromAlarm(uint8_t percent)
{
    s_alarm_target_pct = (percent > 100U) ? 100U : percent;
}

/**
 * @brief 将目标值写入两路 PWM。
 * @param 无。
 * @return 无。
 * @note AlarmEngineTask 是唯一调用者，保证执行器单写入点。
 */
void FANCTRL_Run(void)
{
    if (FAN_PWM_IsReady() == 0U)
    {
        s_actual_pct = 0U;
        return;
    }

    if (s_actual_pct == s_alarm_target_pct)
    {
        return;
    }

    FAN_PWM_SetDuty(1U, s_alarm_target_pct);
    FAN_PWM_SetDuty(2U, s_alarm_target_pct);
    s_actual_pct = s_alarm_target_pct;
}

/**
 * @brief 获取双风扇当前实际占空比。
 * @param 无。
 * @return 0~100%。
 * @note 返回最近一次已下发到两路 PWM 的共同值。
 */
uint8_t FANCTRL_GetActualDuty(void)
{
    return s_actual_pct;
}
