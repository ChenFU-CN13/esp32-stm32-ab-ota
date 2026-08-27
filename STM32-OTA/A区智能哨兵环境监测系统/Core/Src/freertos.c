
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
#include "uart2_comm.h"
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
	
	/*APP_GUI初始化*/
	setup_ui(&guider_ui);
  	events_init(&guider_ui);
	custom_init(&guider_ui);
	/*APP_GUI初始化*/
	
	
	AppTasks_Create();//APP传感器任务启动
	
	UART2_Comm_Start();//USART2通讯任务：业务ACK和OTA通知统一在此处理
	
  /* USER CODE END Init */
		

  /* 创建defaultTask线程 */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);


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
	sensor_Data_t recv_data = {0};
	APP_Watchdog_Refresh();
	
	/******************注册回调事件*******************/
	 
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

	// 新增：风扇调速进度条回调（test4 UI 新增控件）
	lv_obj_add_event_cb(guider_ui.screen_bar_fan,//指定控件
						screen_bar_fan_event_cb, //指定回调函数
						LV_EVENT_VALUE_CHANGED, //检测进度条数值变化
						NULL);

	for(;;)
  {
	/******************DTH11传感器UI更新*******************/
	 // 检查队列中是否有数据（非阻塞，超时时间0）

	
		if(xQueueReceive(xUI_Datas_Queue, &recv_data, 0)==pdTRUE)//非阻塞
        {
			            // 接收到数据，更新UI（控件名已在 test4 中重命名 *_num）
            /* 最终主题将单位拆成独立标签，此处只更新传感器数值。 */
            lv_label_set_text_fmt(guider_ui.screen_humidity_num, "%d", recv_data.humi);
            lv_label_set_text_fmt(guider_ui.screen_temperature_num, "%d", recv_data.temp);
        }

		 
		
		lv_timer_handler(); /* 处理LVGL定时器和任务（必须周期性调用） */
		/* 由现有LVGL主循环承担APP存活证明；任务卡死或调度失效时停止喂狗。 */
		APP_Watchdog_Refresh();
		osDelay(5);

  }
  /* USER CODE END StartDefaultTask */
}
