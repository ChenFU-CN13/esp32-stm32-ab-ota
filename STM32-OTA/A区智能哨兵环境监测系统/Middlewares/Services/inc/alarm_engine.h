#ifndef __ALARM_ENGINE_H
#define __ALARM_ENGINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t smoke_adc;
    int16_t temp_x10;
    int16_t humi;
    uint16_t lux;
} alarm_input_t;

typedef struct
{
    uint8_t red_led_on;
    uint8_t buzzer_on;
    uint8_t fan_pct;
    uint8_t green_led_on;
    uint8_t alarm_level;
} alarm_output_t;

/**
 * @brief 初始化报警输入、输出和 2 秒去抖状态。
 * @param 无。
 * @return 无。
 * @note 初始化后全部执行器输出为关闭。
 */
void ALARM_Init(void);

/**
 * @brief 一次性更新完整报警输入。
 * @param input 输入结构体有效指针。
 * @return 无。
 * @note 复制过程在临界区内完成。
 */
void ALARM_UpdateInput(const alarm_input_t *input);

/**
 * @brief 更新温湿度输入。
 * @param temp_x10 温度乘 10。
 * @param humi 湿度百分数。
 * @return 无。
 * @note 用于 DHT11Task，避免覆盖其他传感器字段。
 */
void ALARM_UpdateDht(int16_t temp_x10, int16_t humi);

/**
 * @brief 更新烟雾 ADC 输入。
 * @param smoke_adc 12 位 ADC 原始值。
 * @return 无。
 * @note 用于 SmokeTask。
 */
void ALARM_UpdateSmoke(uint16_t smoke_adc);

/**
 * @brief 更新光照输入。
 * @param lux 当前照度整数。
 * @return 无。
 * @note 用于 LightTask。
 */
void ALARM_UpdateLux(uint16_t lux);

/**
 * @brief 按当前输入、阈值和用户开关计算最终执行器输出。
 * @param now_ms 当前 HAL 毫秒时基。
 * @param illumination_enabled 1=允许光照自动控制绿灯，0=强制绿灯关闭。
 * @param manual_red_on 无烟雾时的手动红灯状态。
 * @param manual_buzzer_on 无烟雾时的手动蜂鸣器状态。
 * @return 无。
 * @note 每 200 ms 调用一次；烟雾优先覆盖手动红灯、蜂鸣器和风扇。
 */
void ALARM_Run(uint32_t now_ms,
               uint8_t illumination_enabled,
               uint8_t manual_red_on,
               uint8_t manual_buzzer_on);

/**
 * @brief 获取报警引擎当前输出指针。
 * @param 无。
 * @return 内部输出对象地址。
 * @note 仅建议在 AlarmEngineTask 单一上下文中使用。
 */
const alarm_output_t *ALARM_GetOutput(void);

/**
 * @brief 原子复制报警输出快照。
 * @param output 接收输出快照的有效指针。
 * @return 无。
 * @note 供其他任务读取一致状态。
 */
void ALARM_GetOutputSnapshot(alarm_output_t *output);

/**
 * @brief 获取当前报警等级。
 * @param 无。
 * @return 0=正常，1=温湿度超限，2=烟雾报警。
 * @note 返回最近一次 ALARM_Run 的结果。
 */
uint8_t ALARM_GetAlarmLevel(void);

#ifdef __cplusplus
}
#endif

#endif
