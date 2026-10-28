
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "lvgl.h"                // 包含LVGL核心头文件
#include "lv_port_disp.h"        // LVGL显示接口初始化
#include "lv_port_indev.h"       // LVGL输入设备接口初始化

//#include "lv_demo_music.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gui_guider.h"
#include "events_init.h"
#include "custom.h"
#include "APP_Tasks.h"
#include "alarm_engine.h"
#include "uart2_comm.h"
#include <stdio.h>

#define APP_UI_VALUE_LOG_PERIOD_MS 2000UL
/* USER CODE END Includes */


lv_ui guider_ui;//LVGL对象UI句柄


/***************LVGL UI 任务声明参数******************/
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 8,
  .priority = osPriorityLow2,//10
};

/* USER CODE BEGIN FunctionPrototypes */
void StartDefaultTask(void *argument);
void MX_FREERTOS_Init(void); 
static void APP_UI_UpdateSensorValue(lv_obj_t *label, uint8_t valid,
                                     uint16_t value);
static void APP_UI_UpdateSensorX10(lv_obj_t *label, uint8_t valid,
                                   int32_t value_x10);
/* USER CODE END FunctionPrototypes */





/**
  * @brief  FreeRTOS初始化函数
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
    lv_init();                                          /* LVGL系统初始化 */
    lv_port_disp_init();                                /* LVGL显示接口初始化，必须在lv_init()之后调用 */
    lv_port_indev_init();                               /* LVGL输入设备接口初始化，必须在lv_init()之后调用 */
	
	/*LVGL_APP_GUI初始化*/
	setup_ui(&guider_ui);		//首页页面初始化，注意:设置页面是在点击设置按钮后才创建
	events_init(&guider_ui);	//LVGL事件初始化
	custom_init(&guider_ui);	//UI控件初始化
	/*APP_GUI初始化*/
	
	
	AppTasks_Create();//APP传感器任务启动
	
	UART2_Comm_Start();//USART2通讯任务：业务ACK和OTA通知统一在此处理
	
  /* USER CODE END Init */
		

  /* 创建defaultTask线程 */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);


}









/**
 * @brief 根据数据有效状态刷新单个传感器数值标签。
 * @param label LVGL数值标签对象。
 * @param valid 1表示当前数据有效，0表示传感器未就绪或读取失败。
 * @param value 当前传感器数值。
 * @return 无。
 * @note 无效时显示NC；精简数字字体缺少的N和C由Montserrat 22字体回退显示。
 */
static void APP_UI_UpdateSensorValue(lv_obj_t *label, uint8_t valid,
                                     uint16_t value)
{
    if (valid != 0U)
    {
        lv_label_set_text_fmt(label, "%u", (unsigned int)value);
    }
    else
    {
        lv_label_set_text(label, "NC");
    }
}

/**
 * @brief 按1位小数刷新温湿度标签。
 * @param label LVGL数值标签对象。
 * @param valid 1表示数据有效，0表示传感器当前不可用。
 * @param value_x10 放大10倍的定点数，308表示30.8。
 * @return 无。
 * @note 全程使用整数运算，避免在STM32上引入浮点格式化开销。
 */
static void APP_UI_UpdateSensorX10(lv_obj_t *label, uint8_t valid,
                                   int32_t value_x10)
{
    int32_t abs_value;

    if (valid == 0U)
    {
        lv_label_set_text(label, "NC");
        return;
    }

    if (value_x10 < 0)
    {
        abs_value = -value_x10;
        lv_label_set_text_fmt(label, "-%ld.%ld",
                              (long)(abs_value / 10L),
                              (long)(abs_value % 10L));
    }
    else
    {
        lv_label_set_text_fmt(label, "%ld.%ld",
                              (long)(value_x10 / 10L),
                              (long)(value_x10 % 10L));
    }
}

/* ********************* Header_StartDefaultTask LVGL UI 交互任务************ */
/**
  * @brief  defaultTask线程用于LVGL UI的更新，以及UI交互事件等
  * @param  argument: 未使用
  * @retval None
  */
void StartDefaultTask(void *argument)
{
  
	/******************局部变量声明*******************/
	sensor_Data_t recv_data = {0};			//用于接收 xUI_Datas_Queue 中的数据
	sensor_Data_t ui_snapshot = {0};		//用于拷贝一份系统后台读取到的数据，后续通过APP_GetSensorSnapshot在LVGLui中显示
	alarm_output_t alarm_output = {0};		//报警引擎最终输出，用于显示PF10照明灯实际状态
	uint32_t last_ui_refresh = 0UL;			//记录上一次刷新扩展 UI 数据的时间，相当于一个非阻塞软件定时器。控制烟雾、光照、风扇和 IoT 状态每隔约 500ms 刷新一次。
	uint32_t last_ui_value_log = 0UL;		//记录上一次打印 UI 数据日志的时间，用来限制串口日志频率
	uint8_t iot_connected;					//用于暂存 ESP32/OneNET 当前是否在线：1：保持连接onenet，显示 ON；0：没有收到心跳或者已经超时，显示 NC
	APP_Watchdog_Refresh();					//喂狗
	
	/******************注册回调事件*******************/
	/* 自动照明默认开启；另外两个手动开关默认关闭。 */
	lv_obj_add_state(guider_ui.screen_sw_light, LV_STATE_CHECKED);
	lv_obj_clear_state(guider_ui.screen_sw_buzzer, LV_STATE_CHECKED);
	lv_obj_clear_state(guider_ui.screen_sw_Alarm_light, LV_STATE_CHECKED);
	lv_bar_set_value(guider_ui.screen_bar_fan, 0, LV_ANIM_OFF);
	lv_label_set_text(guider_ui.screen_fan_num, "0%");
	//这三个状态每隔 200ms 交给报警程序：
	g_illumination_enabled = 1U;	//照明灯控制：1为受BH1750光照值自动控制开关PF10
	g_buzzer_manual_sw = 0U;		//蜂鸣器控制：0关，1开（如果发生烟雾报警，报警程序会强制蜂鸣器响。即使这里是 0，烟雾报警也能正常响。）
	g_alarm_light_sw = 0U;			//报警灯控制：0关，1开（如果发生烟雾报警，报警程序会接管红灯，让红灯闪烁。烟雾报警结束后，再恢复手动开关状态）
	custom_update_control_status(false, true, false);//设备控制板块3个状态下方显示开关状态
	 
    lv_obj_add_event_cb(guider_ui.screen_sw_light,//指定控件
						screen_sw_light_event_cb, //指定回调函数
						LV_EVENT_VALUE_CHANGED, //检测控件状态变化
						NULL);
	
	lv_obj_add_event_cb(guider_ui.screen_sw_buzzer,//指定控件
						screen_sw_buzzer_event_cb, //指定回调函数
						LV_EVENT_VALUE_CHANGED, //检测控件状态变化
						NULL);

	// 新增：报警灯开关回调（test4 UI 新增控件）
	lv_obj_add_event_cb(guider_ui.screen_sw_Alarm_light,//指定控件
						screen_sw_Alarm_light_event_cb, //指定回调函数
						LV_EVENT_VALUE_CHANGED, //检测控件状态变化
						NULL);

	for(;;)
  {
	/******************SHT30温湿度UI更新*******************/
	 // 检查队列中是否有数据（非阻塞，超时时间0）

	
        if(xQueueReceive(xUI_Datas_Queue, &recv_data, 0)==pdTRUE)//非阻塞
        {
			            // 接收到数据，更新UI（控件名已在 test4 中重命名 *_num）
            /* 最终主题将单位拆成独立标签，此处只更新传感器数值。 */
            APP_UI_UpdateSensorX10(guider_ui.screen_humidity_num,
                                   recv_data.valid_mask & SENSOR_VALID_TEMP_HUMI,
                                   (int32_t)recv_data.humi_x10);
            APP_UI_UpdateSensorX10(guider_ui.screen_temperature_num,
                                   recv_data.valid_mask & SENSOR_VALID_TEMP_HUMI,
                                   (int32_t)recv_data.temp_x10);
        }

		/* 设置页由生成代码按需创建，控件出现后再绑定一次业务回调。 */
		APP_SettingsUI_BindIfReady();

		/* 首页扩展值统一 500 ms 刷新；风扇条只显示报警引擎最终占空比。 */
		if ((uint32_t)(HAL_GetTick() - last_ui_refresh) >= 500UL)
		{
			last_ui_refresh = HAL_GetTick();
			iot_connected = APP_IotHeartbeatCheck(last_ui_refresh);//IOT心跳检查
			APP_GetSensorSnapshot(&ui_snapshot);		//提供数据给UI
			ALARM_GetOutputSnapshot(&alarm_output);		//提供数据给警报仲裁判断
			custom_update_light_output_status(alarm_output.green_led_on != 0U);

			APP_UI_UpdateSensorX10(guider_ui.screen_temperature_num,
							 ui_snapshot.valid_mask & SENSOR_VALID_TEMP_HUMI,
							 (int32_t)ui_snapshot.temp_x10);
			APP_UI_UpdateSensorX10(guider_ui.screen_humidity_num,
							 ui_snapshot.valid_mask & SENSOR_VALID_TEMP_HUMI,
							 (int32_t)ui_snapshot.humi_x10);
			lv_label_set_text_fmt(guider_ui.screen_smoke_num,
							  "%u",
							  (unsigned int)ui_snapshot.smoke_adc);
			APP_UI_UpdateSensorValue(guider_ui.screen_sunlight_num,
							 ui_snapshot.valid_mask & SENSOR_VALID_LIGHT,
							 ui_snapshot.lux);
			lv_label_set_text_fmt(guider_ui.screen_fan_num,
							  "%u%%",
							  (unsigned int)ui_snapshot.fan_pct);
			lv_bar_set_value(guider_ui.screen_bar_fan,
							 (int32_t)ui_snapshot.fan_pct * 10,
							 LV_ANIM_OFF);
			lv_label_set_text(guider_ui.screen_iot_flag,
						  (iot_connected != 0U) ? "ON" : "NC");
			lv_obj_set_style_text_color(guider_ui.screen_iot_flag,
								lv_color_hex((iot_connected != 0U) ? 0x0ac75d : 0xe74c3c),
								LV_PART_MAIN | LV_STATE_DEFAULT);

			/* 新增打印调试内容,测试完成后可删除
			直接读取标签当前文本，确保串口值与屏幕中LVGLui内实际显示值一致。
			 */
			if ((uint32_t)(last_ui_refresh - last_ui_value_log) >= APP_UI_VALUE_LOG_PERIOD_MS)
			{
				last_ui_value_log = last_ui_refresh;
				printf("[UI][DATA][%lu ms] temp=%s C, humi=%s %%, light=%s lx, smoke=%s %%\r\n",
					   (unsigned long)last_ui_refresh,
					   lv_label_get_text(guider_ui.screen_temperature_num),
					   lv_label_get_text(guider_ui.screen_humidity_num),
					   lv_label_get_text(guider_ui.screen_sunlight_num),
					   lv_label_get_text(guider_ui.screen_smoke_num));
			}
		}

		lv_timer_handler(); /* 处理LVGL定时器和任务（必须周期性调用） */

		APP_Watchdog_Refresh();/* 由现有LVGL主循环承担APP存活证明；任务卡死或调度失效时停止喂狗。 */

		osDelay(5);

  }
  /* USER CODE END StartDefaultTask */
}
