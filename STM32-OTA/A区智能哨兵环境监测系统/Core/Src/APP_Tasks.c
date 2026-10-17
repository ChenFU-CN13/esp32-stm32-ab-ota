#include "APP_Tasks.h"
#include "modbus_th.h"
#include "buzzer.h"
#include "mq2.h"
#include "bh1750.h"
#include "fan_pwm.h"
#include "alarm_engine.h"
#include "fan_controller.h"
#include "config_manager.h"
#include "custom.h"
#include "./BSP/LED/led.h"
#include <stdio.h>
#include <string.h>

#define APP_SHT30_PERIOD_MS            1000    /**< SHT30温湿度传感器采集周期，单位ms */
#define APP_SMOKE_PERIOD_MS             500U    /**< 烟雾传感器采集周期，单位ms，500ms即0.5秒 */
#define APP_MQ2_PREHEAT_MS           1200000UL   /**< MQ-2上电预热时间（大概16分钟），预热期间只采集、不参与报警判断 */
#define APP_LIGHT_PERIOD_MS            1000U    /**< 光照传感器采集周期，单位ms，1000ms即1秒 */
#define APP_ALARM_PERIOD_MS             200U    /**< 报警器告警逻辑处理周期，单位ms，200ms */
#define APP_PROPERTY_REPORT_CYCLES       25U    /**< IoT设备属性上报触发周期，经过N次采集后上报一次云端 */
#define APP_IOT_TIMEOUT_MS            15000UL   /**< IoT云端通信超时时间，单位ms，15000ms即15秒 */

/* 全局传感器数据结构体，存放各类传感器采集到的数据 */
sensor_Data_t g_sensor_data = {0};

/* UI数据队列：任务向LVGL界面推送显示数据的FreeRTOS队列句柄 */
QueueHandle_t xUI_Datas_Queue = NULL;
/* UART2发送队列：串口2待发送消息的FreeRTOS队列句柄 */
QueueHandle_t xUART2_TxQueue = NULL;

/* 自动照明总开关，默认开启，由BH1750光照值控制PF10 */
volatile uint8_t g_illumination_enabled = 1U;
/* 告警灯光开关标志，中断/多任务会修改 */
volatile uint8_t g_alarm_light_sw = 0U;
/* 蜂鸣器手动控制开关标志 */
volatile uint8_t g_buzzer_manual_sw = 0U;
/* 风扇PWM占空比参数 */
volatile uint8_t g_fan_pwm = 0U;
/* IoT云端连接状态标志：1已连接，0断开 */
volatile uint8_t g_iot_connected = 0U;
/* 上一次IoT心跳包的系统tick计数，用于心跳超时判断 */
volatile uint32_t g_last_heartbeat_tick = 0UL;

/* 静态任务句柄，仅本文件内部使用，各个传感器、告警任务的任务句柄 */
static TaskHandle_t s_sht30_task_handle = NULL;      /* SHT30温湿度任务句柄 */
static TaskHandle_t s_smoke_task_handle = NULL;       /* 烟雾检测任务句柄 */
static TaskHandle_t s_light_task_handle = NULL;       /* 光照采集任务句柄 */
static TaskHandle_t s_alarm_task_handle = NULL;       /* 告警引擎业务任务句柄 */

/* IoT心跳收到标记，仅本文件内部使用 */
static uint8_t s_iot_heartbeat_seen = 0U;

/* LVGL对象指针：绑定设置界面屏幕对象，仅本文件内访问 */
static lv_obj_t *s_bound_setting_screen = NULL;

/* 设置页面可编辑阈值参数，本地保存用户配置 */
static int16_t s_edit_temp = 28;        /* 告警温度阈值默认28℃ */
static int16_t s_edit_humi = 80;        /* 告警湿度阈值默认80%RH */
static uint16_t s_edit_smoke = 1500U;   /* 烟雾告警阈值默认1500 */
static uint16_t s_edit_light_low = 5U;  /* 光照低于5lx时开灯 */
static uint16_t s_edit_light_high = 20U;/* 光照高于20lx时关灯 */


/************************ 本文件内部函数声明(static，外部不可调用) ************************/
/**
 * @brief SHT30温湿度采集任务函数
 * @param parameters 任务创建传入参数
 */
static void SHT30Task(void *parameters);

/**
 * @brief 烟雾传感器采集任务函数
 * @param parameters 任务创建传入参数
 */
static void SmokeTask(void *parameters);

/**
 * @brief 光照传感器采集任务函数
 * @param parameters 任务创建传入参数
 */
static void LightTask(void *parameters);

/**
 * @brief 告警引擎任务，处理报警逻辑、蜂鸣器、风扇、灯光联动
 * @param parameters 任务创建传入参数
 */
static void AlarmEngineTask(void *parameters);

/**
 * @brief 将串口消息压入UART2发送队列
 * @param message 待发送的串口消息结构体指针
 * @return pdPASS成功 / pdFALSE队列入队失败
 */
static BaseType_t APP_QueueUartMessage(const UART_Msg_t *message);

/**
 * @brief 从非易失配置区加载用户阈值配置到内存变量
 */
static void APP_SettingsLoadFromConfig(void);

/**
 * @brief 将内存中的阈值参数刷新到LVGL界面显示控件
 */
static void APP_SettingsRefreshLabels(void);

/**
 * @brief LVGL设置界面数值调整事件回调
 * @param event lvgl事件句柄
 */
static void APP_SettingsAdjustEvent(lv_event_t *event);

/**
 * @brief LVGL页面"保存"按钮事件回调，保存配置到存储介质
 * @param event lvgl事件句柄
 */
static void APP_SettingsSaveEvent(lv_event_t *event);

/**
 * @brief LVGL"恢复默认"按钮事件回调，恢复出厂阈值
 * @param event lvgl事件句柄
 */
static void APP_SettingsDefaultEvent(lv_event_t *event);
/************************************************/

/**
 * @brief 将一条业务消息放入 UART2 队列。
 * @param message 待发送消息有效指针。
 * @return pdPASS=成功，其他=队列不可用或已满。
 * @note 不阻塞调用任务，可靠 ACK 与重试由 UART2_Comm_Task 完成。
 */
static BaseType_t APP_QueueUartMessage(const UART_Msg_t *message)
{
    if ((xUART2_TxQueue == NULL) || (message == NULL))
    {
        return pdFAIL;
    }
    return xQueueSendToBack(xUART2_TxQueue, message, 0U);
}


/*补充说明：在传感器任务获取数据代码部分，会加入RTOS的全局数据的原子更新（临界区保护）
1.`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`：FreeRTOS 任务级临界区，
作用是**关闭任务调度**，保证这段代码不会被其他任务打断

**为什么用临界区**：
`g_sensor_data` 是全局共享结构体，可能被多个任务 / 中断读取。
如果没有临界区，可能发生 “数据撕裂”—— 刚更新了 `temp`，还没更新 `humi`，就被其他任务读走，得到一组不一致的数据
具体代码如下：
            taskENTER_CRITICAL();
            g_sensor_data.temp = temp;
            g_sensor_data.humi = humi;
            g_sensor_data.valid_mask |= SENSOR_VALID_DHT11;
            snapshot = g_sensor_data;
            taskEXIT_CRITICAL();

。
**快照设计**：
在临界区内把 `g_sensor_data` 完整拷贝到 `snapshot`，后续的报警、入队、上报全部使用 `snapshot`，不再访问全局变量。
好处：既保证了数据一致性，又**最大限度缩短了临界区时长**（临界区只做赋值和拷贝，耗时极短），是嵌入式多任务编程的最佳实践之一。
*/

/**
 * @brief SHT30 温湿度采集任务。
 * @param parameters 未使用。
 * @return 无，任务永久运行。
 * @note 每2秒读取一次。内部保留1位小数，UI直接显示；UART2上报时再转成整数。
 */
static void SHT30Task(void *parameters)
{
    MBTH_FixedData_t data;
    MBTH_Status_t status;
    sensor_Data_t snapshot;
    UART_Msg_t message;
    TickType_t last_wake_time;
    uint16_t humi_integer;
    uint8_t read_failed = 0U;

    (void)parameters;
    last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        status = MBTH_ReadFixed(&huart3, MBTH_DEFAULT_ADDR, &data);
        if ((status == MBTH_OK) && (data.humi_x10 <= 1000U))
        {
            /* 温湿度必须成组更新，防止其他任务读到一新一旧的组合。 */
            taskENTER_CRITICAL();
            g_sensor_data.temp_x10 = data.temp_x10;
            g_sensor_data.humi_x10 = data.humi_x10;
            g_sensor_data.valid_mask |= SENSOR_VALID_TEMP_HUMI;
            snapshot = g_sensor_data;
            taskEXIT_CRITICAL();

            /* 报警阈值仍使用整数湿度，四舍五入后再交给报警引擎。 */
            humi_integer = (uint16_t)((data.humi_x10 + 5U) / 10U);
            ALARM_UpdateTempHumi(data.temp_x10, (int16_t)humi_integer, 1U);

            /* UI队列长度为1，只保留最新快照，不积压历史采样值。 */
			/*- 使用 `xQueueOverwrite` 向 UI 队列写入数据，这是 FreeRTOS 专门为 “只需要最新值” 场景设计的 API：
            - 队列满时，直接覆盖最旧的数据，永远不会阻塞；
            - 非常适合 UI 刷新场景 —— 显示屏只需要显示当前最新的温湿度，积压的历史数据没有意义。
            - 先判断队列句柄非空，增强鲁棒性，避免队列未创建时跑飞。*/
            if (xUI_Datas_Queue != NULL)
            {
                (void)xQueueOverwrite(xUI_Datas_Queue, &snapshot);
            }

            memset(&message, 0, sizeof(message));
            message.type = MSG_TYPE_TEMP_HUMI;
            message.data.temp_humi_data = snapshot;
            if (APP_QueueUartMessage(&message) != pdPASS)
            {
                printf("[UART2] temp/humi report queue full\r\n");
            }

            if (read_failed != 0U)
            {
                printf("[SENSOR][SHT30] recovered\r\n");
                read_failed = 0U;
            }
        }
        else
        {
            /* 保留最后一次数值便于排查，但清除有效位，UI显示NC，报警不再使用旧数据。 */
            taskENTER_CRITICAL();
            g_sensor_data.valid_mask &= (uint8_t)(~SENSOR_VALID_TEMP_HUMI);
            snapshot = g_sensor_data;
            taskEXIT_CRITICAL();
            ALARM_UpdateTempHumi(0, 0, 0U);

            if (xUI_Datas_Queue != NULL)
            {
                (void)xQueueOverwrite(xUI_Datas_Queue, &snapshot);
            }

            if (read_failed == 0U)
            {
                if (status == MBTH_OK)
                {
                    printf("[SENSOR][SHT30] invalid data, T=%d, H=%u\r\n",
                           (int)data.temp_x10,
                           (unsigned int)data.humi_x10);
                }
                else
                {
                    printf("[SENSOR][SHT30] read FAIL, status=%d\r\n", (int)status);
                }
            }
            read_failed = 1U;
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(APP_SHT30_PERIOD_MS));
    }
}

/**
 * @brief MQ-2 烟雾采集任务。
 * @param parameters 未使用。
 * @return 无，任务永久运行。
 * @note 每 500 ms 读取 5 次采样中位数；前 3 分钟只更新采集值，不送入烟雾报警判断。
 */
static void SmokeTask(void *parameters)
{
    uint16_t smoke_adc;
    uint32_t preheat_start_ms;
    uint8_t read_failed = 0U;
    uint8_t retry_count = 0U;

    (void)parameters;
    preheat_start_ms = HAL_GetTick();
    printf("[SENSOR][MQ2] init %s\r\n", (MQ2_IsReady() != 0U) ? "OK" : "FAIL");
    printf("[SENSOR][MQ2] preheat 180 s, alarm disabled, sampling active\r\n");

    for (;;)
    {
        smoke_adc = MQ2_ReadFiltered(5U);
        if (smoke_adc != MQ2_INVALID_RAW)
        {
            taskENTER_CRITICAL();
            g_sensor_data.smoke_adc = smoke_adc;
            taskEXIT_CRITICAL();
            if ((uint32_t)(HAL_GetTick() - preheat_start_ms) >= APP_MQ2_PREHEAT_MS)
            {
                ALARM_UpdateSmoke(smoke_adc);
            }
            retry_count = 0U;
            if (read_failed != 0U)
            {
                printf("[SENSOR][MQ2] recovered\r\n");
                read_failed = 0U;
            }
        }
        else
        {
            if (read_failed == 0U)
            {
                printf("[SENSOR][MQ2] read FAIL\r\n");
                read_failed = 1U;
            }
            retry_count++;
            if (retry_count >= 10U)
            {
                MQ2_Init();
                retry_count = 0U;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_SMOKE_PERIOD_MS));
    }
}

/**
 * @brief BH1750 光照采集任务。
 * @param parameters 未使用。
 * @return 无，任务永久运行。
 * @note 每 1 秒读取一次连续高分辨率结果；连续 5 次失败后重新初始化总线与传感器。
 */
static void LightTask(void *parameters)
{
    uint16_t lux;
    uint8_t read_failed = 0U;
    uint8_t retry_count = 0U;

    (void)parameters;
    printf("[SENSOR][BH1750] init %s\r\n", (BH1750_IsReady() != 0U) ? "OK" : "FAIL");

    for (;;)
    {
        if (BH1750_ReadLux(&lux) == 0U)
        {
            taskENTER_CRITICAL();
            g_sensor_data.lux = lux;
            g_sensor_data.valid_mask |= SENSOR_VALID_LIGHT;
            taskEXIT_CRITICAL();
            ALARM_UpdateLux(lux);
            retry_count = 0U;
            if (read_failed != 0U)
            {
                printf("[SENSOR][BH1750] recovered\r\n");
                read_failed = 0U;
            }
        }
        else
        {
            taskENTER_CRITICAL();
            g_sensor_data.valid_mask &= (uint8_t)(~SENSOR_VALID_LIGHT);
            taskEXIT_CRITICAL();

            if (read_failed == 0U)
            {
                printf("[SENSOR][BH1750] read FAIL\r\n");
                read_failed = 1U;
            }
            retry_count++;
            if (retry_count >= 5U)
            {
                BH1750_Init();
                retry_count = 0U;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_LIGHT_PERIOD_MS));
    }
}

/**
 * @brief 报警决策、执行器写入和扩展属性汇总任务。
 * @param parameters 未使用。
 * @return 无，任务永久运行。
 * @note 200 ms 固定周期；本任务是 PF8/PF9/PF10 和双风扇 PWM 的唯一最终写入者。
 */
static void AlarmEngineTask(void *parameters)
{
    TickType_t last_wake_tick;
    alarm_output_t output;
    sensor_Data_t snapshot;
    UART_Msg_t message;
    uint8_t previous_level = 0xFFU;
    uint8_t previous_red = 0xFFU;
    uint8_t previous_buzzer = 0xFFU;
    uint8_t previous_green = 0xFFU;
    uint8_t previous_alarm_report = 0xFFU;
    uint8_t report_cycle = 0U;
    uint8_t actual_fan;
    uint8_t logical_alarm;
    uint8_t queue_failed;

    (void)parameters;
    last_wake_tick = xTaskGetTickCount();

    for (;;)
    {
        ALARM_Run(HAL_GetTick(),
                  g_illumination_enabled,
                  g_alarm_light_sw,
                  g_buzzer_manual_sw);
        ALARM_GetOutputSnapshot(&output);

        FANCTRL_UpdateFromAlarm(output.fan_pct);
        FANCTRL_Run();
        actual_fan = FANCTRL_GetActualDuty();

        if (output.red_led_on != previous_red)
        {
            HAL_GPIO_WritePin(GPIOF,
                              GPIO_PIN_9,
                              (output.red_led_on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
            previous_red = output.red_led_on;
        }
        if (output.buzzer_on != previous_buzzer)
        {
            HAL_GPIO_WritePin(GPIOF,
                              GPIO_PIN_8,
                              (output.buzzer_on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            previous_buzzer = output.buzzer_on;
        }
        if (output.green_led_on != previous_green)
        {
            HAL_GPIO_WritePin(GPIOF,
                              GPIO_PIN_10,
                              (output.green_led_on != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
            previous_green = output.green_led_on;

            memset(&message, 0, sizeof(message));
            message.type = MSG_TYPE_LED;
            message.data.led_cmd = (output.green_led_on != 0U) ? LED_CMD_ON : LED_CMD_OFF;
            (void)APP_QueueUartMessage(&message);
        }

        logical_alarm = ((output.alarm_level == 2U) || (g_buzzer_manual_sw != 0U)) ? 1U : 0U;
        if (logical_alarm != previous_alarm_report)
        {
            memset(&message, 0, sizeof(message));
            message.type = MSG_TYPE_BUZZER;
            message.data.buzzer_cmd = (logical_alarm != 0U) ? buzzer_CMD_ON : buzzer_CMD_OFF;
            (void)APP_QueueUartMessage(&message);
            previous_alarm_report = logical_alarm;
        }

        taskENTER_CRITICAL();
        g_fan_pwm = actual_fan;
        g_sensor_data.fan_pct = actual_fan;
        taskEXIT_CRITICAL();

        if (output.alarm_level != previous_level)
        {
            printf("[ALARM] level %u -> %u, fan=%u%%\r\n",
                   (previous_level == 0xFFU) ? 0U : (unsigned int)previous_level,
                   (unsigned int)output.alarm_level,
                   (unsigned int)actual_fan);
            previous_level = output.alarm_level;
        }

        report_cycle++;
        if (report_cycle >= APP_PROPERTY_REPORT_CYCLES)
        {
            report_cycle = 0U;
            APP_GetSensorSnapshot(&snapshot);
            queue_failed = 0U;

            memset(&message, 0, sizeof(message));
            message.type = MSG_TYPE_SMOKE;
            message.data.smoke_adc = snapshot.smoke_adc;
            if (APP_QueueUartMessage(&message) != pdPASS)
            {
                queue_failed = 1U;
            }

            memset(&message, 0, sizeof(message));
            message.type = MSG_TYPE_LIGHT;
            message.data.lux = snapshot.lux;
            if (APP_QueueUartMessage(&message) != pdPASS)
            {
                queue_failed = 1U;
            }

            memset(&message, 0, sizeof(message));
            message.type = MSG_TYPE_FAN;
            message.data.fan_pct = snapshot.fan_pct;
            if (APP_QueueUartMessage(&message) != pdPASS)
            {
                queue_failed = 1U;
            }

        }

        vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(APP_ALARM_PERIOD_MS));
    }
}

/**
 * @brief 获取当前传感器与执行器显示快照。
 * @param snapshot 接收数据的有效指针。
 * @return 无。
 * @note 心跳状态在复制时同步写入快照字段。
 */
void APP_GetSensorSnapshot(sensor_Data_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    *snapshot = g_sensor_data;
    snapshot->iot_connected = g_iot_connected;
    taskEXIT_CRITICAL();
}

/**
 * @brief 更新 ESP32 心跳状态。
 * @param connected 1=在线，0=离线。
 * @param tick_ms 收到帧时的 HAL 毫秒时基。
 * @return 无。
 * @note 日志仅在首次收到心跳或在线状态变化时输出。
 */
void APP_IotHeartbeatUpdate(uint8_t connected, uint32_t tick_ms)
{
    uint8_t normalized = (connected != 0U) ? 1U : 0U;
    uint8_t previous = g_iot_connected;

    taskENTER_CRITICAL();
    g_iot_connected = normalized;
    g_last_heartbeat_tick = tick_ms;
    taskEXIT_CRITICAL();

    if ((s_iot_heartbeat_seen == 0U) || (previous != normalized))
    {
        printf("[UART2][RX][HEARTBEAT] %s\r\n", (normalized != 0U) ? "ON" : "OFF");
        s_iot_heartbeat_seen = 1U;
    }
}

/**
 * @brief 检查 15 秒心跳超时。
 * @param now_ms 当前 HAL 毫秒时基。
 * @return 1=在线，0=离线。
 * @note 仅从在线切到离线时打印一次日志。
 */
uint8_t APP_IotHeartbeatCheck(uint32_t now_ms)
{
    if ((g_iot_connected != 0U) &&
        ((uint32_t)(now_ms - g_last_heartbeat_tick) > APP_IOT_TIMEOUT_MS))
    {
        taskENTER_CRITICAL();
        g_iot_connected = 0U;
        taskEXIT_CRITICAL();
        printf("[UART2][RX][HEARTBEAT] timeout -> OFF\r\n");
    }

    return g_iot_connected;
}

/**
 * @brief 照明自动控制总开关回调。
 * @param event LVGL 事件。
 * @return 无。
 * @note 默认开启；回调只更新意图，最终 PF10 输出由 AlarmEngineTask 写入。
 */
void screen_sw_light_event_cb(lv_event_t *event)
{
    lv_obj_t *switch_obj = lv_event_get_target(event);
    g_illumination_enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED) ? 1U : 0U;
    custom_update_control_status(g_buzzer_manual_sw != 0U,
                                 g_illumination_enabled != 0U,
                                 g_alarm_light_sw != 0U);
    printf("[UI] illumination auto=%s\r\n", (g_illumination_enabled != 0U) ? "ON" : "OFF");
}

/**
 * @brief 手动蜂鸣器开关回调。
 * @param event LVGL 事件。
 * @return 无。
 * @note 烟雾报警优先级更高，手动意图会保留到报警解除后恢复。
 */
void screen_sw_buzzer_event_cb(lv_event_t *event)
{
    lv_obj_t *switch_obj = lv_event_get_target(event);
    g_buzzer_manual_sw = lv_obj_has_state(switch_obj, LV_STATE_CHECKED) ? 1U : 0U;
    custom_update_control_status(g_buzzer_manual_sw != 0U,
                                 g_illumination_enabled != 0U,
                                 g_alarm_light_sw != 0U);
    printf("[UI] buzzer manual=%s\r\n", (g_buzzer_manual_sw != 0U) ? "ON" : "OFF");
}

/**
 * @brief 手动红色报警灯开关回调。
 * @param event LVGL 事件。
 * @return 无。
 * @note 烟雾报警优先级更高，手动意图会保留到报警解除后恢复。
 */
void screen_sw_Alarm_light_event_cb(lv_event_t *event)
{
    lv_obj_t *switch_obj = lv_event_get_target(event);
    g_alarm_light_sw = lv_obj_has_state(switch_obj, LV_STATE_CHECKED) ? 1U : 0U;
    custom_update_control_status(g_buzzer_manual_sw != 0U,
                                 g_illumination_enabled != 0U,
                                 g_alarm_light_sw != 0U);
    printf("[UI] alarm light manual=%s\r\n", (g_alarm_light_sw != 0U) ? "ON" : "OFF");
}

/**
 * @brief 从当前配置装载设置页编辑缓存。
 * @param 无。
 * @return 无。
 * @note 光照下限没有独立 UI 控件，装载后保存时保持原值。
 */
static void APP_SettingsLoadFromConfig(void)
{
    app_config_t config;

    CFG_GetSnapshot(&config);
    s_edit_temp = (int16_t)(config.temp_high_x10 / 10);
    s_edit_humi = config.humi_high;
    s_edit_smoke = config.smoke_adc_thr;
    s_edit_light_low = config.light_low;
    s_edit_light_high = config.light_high;
    APP_SettingsRefreshLabels();
}

/**
 * @brief 将编辑缓存刷新到当前设置页标签。
 * @param 无。
 * @return 无。
 * @note 仅在 LVGL 任务上下文调用。
 */
static void APP_SettingsRefreshLabels(void)
{
    if (guider_ui.screen_setting_num_temp != NULL)
    {
        lv_label_set_text_fmt(guider_ui.screen_setting_num_temp, "%d", (int)s_edit_temp);
    }
    if (guider_ui.screen_setting_num_hum != NULL)
    {
        lv_label_set_text_fmt(guider_ui.screen_setting_num_hum, "%d", (int)s_edit_humi);
    }
    if (guider_ui.screen_setting_num_sunlight != NULL)
    {
        lv_label_set_text_fmt(guider_ui.screen_setting_num_sunlight,
                              "%u",
                              (unsigned int)s_edit_light_high);
    }
    if (guider_ui.screen_setting_num_smoke != NULL)
    {
        lv_label_set_text_fmt(guider_ui.screen_setting_num_smoke,
                              "%u",
                              (unsigned int)s_edit_smoke);
    }
}

/**
 * @brief 处理设置页四组加减按钮。
 * @param event LVGL 点击事件；user_data 编码字段和增减方向。
 * @return 无。
 * @note 温度步进 1、湿度步进 1、光照上限步进 10、烟雾阈值步进 10。
 */
static void APP_SettingsAdjustEvent(lv_event_t *event)
{
    int32_t action = (int32_t)(intptr_t)lv_event_get_user_data(event);

    switch (action)
    {
        case 1:
            if (s_edit_temp < 60) s_edit_temp++;
            break;
        case -1:
            if (s_edit_temp > -10) s_edit_temp--;
            break;
        case 2:
            if (s_edit_humi < 100) s_edit_humi++;
            break;
        case -2:
            if (s_edit_humi > 20) s_edit_humi--;
            break;
        case 3:
            if (s_edit_light_high <= 65525U) s_edit_light_high = (uint16_t)(s_edit_light_high + 10U);
            break;
        case -3:
            if (s_edit_light_high > (uint16_t)(s_edit_light_low + 10U))
            {
                s_edit_light_high = (uint16_t)(s_edit_light_high - 10U);
            }
            break;
        case 4:
            if (s_edit_smoke <= 4085U) s_edit_smoke = (uint16_t)(s_edit_smoke + 10U);
            else s_edit_smoke = 4095U;
            break;
        case -4:
            if (s_edit_smoke >= 10U) s_edit_smoke = (uint16_t)(s_edit_smoke - 10U);
            else s_edit_smoke = 0U;
            break;
        default:
            break;
    }

    APP_SettingsRefreshLabels();
}

/**
 * @brief 保存设置页编辑值。
 * @param event LVGL 点击事件，未使用。
 * @return 无。
 * @note 四个 Setter 只改 RAM，最后仅调用一次 CFG_Save 写 16 字节并读回校验。
 */
static void APP_SettingsSaveEvent(lv_event_t *event)
{
    uint8_t result;

    (void)event;
    CFG_SetTempHigh((int16_t)(s_edit_temp * 10));
    CFG_SetHumiHigh(s_edit_humi);
    CFG_SetSmokeThr(s_edit_smoke);
    CFG_SetLightRange(s_edit_light_low, s_edit_light_high);
    result = CFG_Save();
    printf("[CFG] save %s\r\n", (result == 0U) ? "OK" : "VERIFY_FAIL");
}

/**
 * @brief 恢复默认设置并刷新设置页。
 * @param event LVGL 点击事件，未使用。
 * @return 无。
 * @note CFG_SetDefault 内部只执行一次写入与读回校验。
 */
static void APP_SettingsDefaultEvent(lv_event_t *event)
{
    uint8_t result;

    (void)event;
    result = CFG_SetDefault();
    APP_SettingsLoadFromConfig();
    printf("[CFG] default %s\r\n", (result == 0U) ? "OK" : "VERIFY_FAIL");
}

/**
 * @brief 在动态设置页创建后注册阈值编辑回调。
 * @param 无。
 * @return 无。
 * @note 同一 screen_setting 对象只绑定一次，不修改 generated/events_init.c。
 */
void APP_SettingsUI_BindIfReady(void)
{
    if ((guider_ui.screen_setting == NULL) ||
        (guider_ui.screen_setting == s_bound_setting_screen))
    {
        return;
    }

    s_bound_setting_screen = guider_ui.screen_setting;
    APP_SettingsLoadFromConfig();

    lv_obj_add_event_cb(guider_ui.screen_setting_add_1, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    lv_obj_add_event_cb(guider_ui.screen_setting_minus_1, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    lv_obj_add_event_cb(guider_ui.screen_setting_add_2, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)2);
    lv_obj_add_event_cb(guider_ui.screen_setting_minus_2, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)-2);
    lv_obj_add_event_cb(guider_ui.screen_setting_add_3, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)3);
    lv_obj_add_event_cb(guider_ui.screen_setting_minus_3, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)-3);
    lv_obj_add_event_cb(guider_ui.screen_setting_add_4, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)4);
    lv_obj_add_event_cb(guider_ui.screen_setting_minus_4, APP_SettingsAdjustEvent, LV_EVENT_CLICKED, (void *)(intptr_t)-4);
    lv_obj_add_event_cb(guider_ui.screen_setting_btn_save, APP_SettingsSaveEvent, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(guider_ui.screen_setting_btn_default, APP_SettingsDefaultEvent, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief 创建 APP 采集、报警任务以及业务消息队列。
 * @param 无。
 * @return 无。
 * @note 先完成配置和硬件初始化，再创建任务，避免任务读到未就绪资源。
 */
void AppTasks_Create(void)
{
    BaseType_t task_result;

    xUART2_TxQueue = xQueueCreate(16U, sizeof(UART_Msg_t));
    configASSERT(xUART2_TxQueue != NULL);
    xUI_Datas_Queue = xQueueCreate(1U, sizeof(sensor_Data_t));
    configASSERT(xUI_Datas_Queue != NULL);

    CFG_Init();
    ALARM_Init();
    led_init();
    beep_init();
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9 | GPIO_PIN_10, GPIO_PIN_SET);//默认双灯关闭
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET);//默认蜂鸣器关闭
    FANCTRL_Init();
    MQ2_Init();
    BH1750_Init();

    printf("[APP][INIT] PWM=%s ADC=%s BH1750=%s, illumination=ON\r\n",
           (FAN_PWM_IsReady() != 0U) ? "OK" : "FAIL",
           (MQ2_IsReady() != 0U) ? "OK" : "FAIL",
           (BH1750_IsReady() != 0U) ? "OK" : "FAIL");

    task_result = xTaskCreate(SHT30Task,
                              "SHT30Task",
                              256U,
                              NULL,
                              4U,
                              &s_sht30_task_handle);
    configASSERT(task_result == pdPASS);


    task_result = xTaskCreate(SmokeTask,
                              "SmokeTask",
                              128U,
                              NULL,
                              4U,
                              &s_smoke_task_handle);
    configASSERT(task_result == pdPASS);

    task_result = xTaskCreate(LightTask,
                              "LightTask",
                              128U,
                              NULL,
                              3U,
                              &s_light_task_handle);
    configASSERT(task_result == pdPASS);

    task_result = xTaskCreate(AlarmEngineTask,
                              "AlarmEngine",
                              256U,
                              NULL,
                              5U,
                              &s_alarm_task_handle);
    configASSERT(task_result == pdPASS);
}
