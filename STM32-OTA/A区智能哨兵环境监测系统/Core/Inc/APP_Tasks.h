#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "gui_guider.h"
#include "cmsis_os.h"
#include <stdint.h>

#define SENSOR_VALID_TEMP_HUMI (1U << 0) /**< 温湿度数据有效 */
#define SENSOR_VALID_LIGHT     (1U << 2) /**< 光照数据有效 */

typedef struct
{
    int16_t temp_x10;        /**< 温度放大10倍，308表示30.8摄氏度 */
    uint16_t humi_x10;       /**< 湿度放大10倍，646表示64.6%RH */
    uint8_t iot_connected;
    uint8_t fan_pct;
    uint8_t valid_mask;      /**< SENSOR_VALID_xxx组合，0表示尚无有效传感器数据 */
    uint16_t smoke_adc;
    uint16_t lux;
} sensor_Data_t; /* 添加新的传感器数据到这个结构体，蜂鸣器和 LED 不属于传感器数据。 */

typedef enum
{
    LED_CMD_OFF = 0,
    LED_CMD_ON
} LED_Cmd_t;

typedef enum
{
    buzzer_CMD_OFF = 0,
    buzzer_CMD_ON
} buzzer_Cmd_t;

typedef struct
{
    uint8_t header;
    uint8_t type;
    uint8_t data[5];
    uint8_t tail;
} UART_Packet_t;    /*STM32与ESP32S3的固定8字节数据格式*/

typedef enum
{
    MSG_TYPE_TEMP_HUMI = 0,
    MSG_TYPE_LED,
    MSG_TYPE_BUZZER,
    MSG_TYPE_FAN,
    MSG_TYPE_SMOKE,
    MSG_TYPE_LIGHT
} UART_MsgType_t;   /*物模型数据类型*/

typedef struct
{
    UART_MsgType_t type;
    union
    {
        sensor_Data_t temp_humi_data;
        LED_Cmd_t led_cmd;
        buzzer_Cmd_t buzzer_cmd;
        uint8_t fan_pct;
        uint16_t smoke_adc;
        uint16_t lux;
    } data;
} UART_Msg_t;   /*各任务送给 UART2 通讯任务的内部消息。*/

extern sensor_Data_t g_sensor_data;
extern QueueHandle_t xUI_Datas_Queue;
extern QueueHandle_t xUART2_TxQueue;

extern volatile uint8_t g_illumination_enabled;
extern volatile uint8_t g_alarm_light_sw;
extern volatile uint8_t g_buzzer_manual_sw;
extern volatile uint8_t g_fan_pwm;
extern volatile uint8_t g_iot_connected;
extern volatile uint32_t g_last_heartbeat_tick;

/**
 * @brief 创建 APP 采集、报警任务以及业务消息队列。
 * @param 无。
 * @return 无。
 * @note 在 FreeRTOS 调度器启动前调用一次。
 */
void AppTasks_Create(void);

/**
 * @brief 获取当前传感器与执行器显示快照。
 * @param snapshot 接收数据的有效指针。
 * @return 无。
 * @note 使用短临界区避免 UI 读到跨字段混合值。
 */
void APP_GetSensorSnapshot(sensor_Data_t *snapshot);

/**
 * @brief 更新 ESP32 心跳状态。
 * @param connected 1=OneNET 在线，0=离线。
 * @param tick_ms 收到心跳时的 HAL 毫秒时基。
 * @return 无。
 * @note 由 UART2 通讯任务调用，日志只在首次收到或状态变化时打印。
 */
void APP_IotHeartbeatUpdate(uint8_t connected, uint32_t tick_ms);

/**
 * @brief 检查 15 秒心跳超时。
 * @param now_ms 当前 HAL 毫秒时基。
 * @return 1=在线，0=离线。
 * @note 使用无符号减法，支持 32 位时基回绕。
 */
uint8_t APP_IotHeartbeatCheck(uint32_t now_ms);

/**
 * @brief 照明自动控制总开关回调。
 * @param event LVGL 事件。
 * @return 无。
 * @note 关闭时 AlarmEngineTask 强制 PF10 熄灭；开启后才按 BH1750 阈值自动控制。
 */
void screen_sw_light_event_cb(lv_event_t *event);

/**
 * @brief 手动蜂鸣器开关回调。
 * @param event LVGL 事件。
 * @return 无。
 * @note 烟雾报警期间由报警引擎覆盖，报警解除后恢复手动状态。
 */
void screen_sw_buzzer_event_cb(lv_event_t *event);

/**
 * @brief 手动红色报警灯开关回调。
 * @param event LVGL 事件。
 * @return 无。
 * @note 烟雾报警期间由报警引擎覆盖，报警解除后恢复手动状态。
 */
void screen_sw_Alarm_light_event_cb(lv_event_t *event);

/**
 * @brief 在动态设置页创建后注册阈值编辑回调。
 * @param 无。
 * @return 无。
 * @note 可在 LVGL 任务循环中重复调用；同一页面对象只绑定一次。
 */
void APP_SettingsUI_BindIfReady(void);

#ifdef __cplusplus
}
#endif

#endif
