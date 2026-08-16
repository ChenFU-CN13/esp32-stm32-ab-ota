
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

#include "lv_demo_music.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gui_guider.h"
#include "events_init.h"
#include "APP_Tasks.h"
/* USER CODE END Includes */


lv_ui guider_ui;//LVGL对象UI句柄


/***************LVGL UI 任务声明参数******************/
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 8,
  .priority = osPriorityLow2,//10
};

/***************串口任务声明参数******************/
static TaskHandle_t USART2_Transmit_TaskHand_t;
 UART_TxState_t g_uart2_tx_state = UART_TX_IDLE;
QueueHandle_t xUART2_PacketQueue = NULL;	//专门存储打包后的数据包


/* USER CODE BEGIN FunctionPrototypes */

void StartDefaultTask(void *argument);
void MX_FREERTOS_Init(void); 
void USART2_Transmit_Task_create(void);
void UART_Packet_Init(UART_Packet_t *packet, uint8_t type);
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
	/*APP_GUI初始化*/
	
	
	AppTasks_Create();//APP传感器任务启动
	
	USART2_Transmit_Task_create();//串口通讯任务
	
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
	
	/******************注册回调事件*******************/
	 
    lv_obj_add_event_cb(guider_ui.screen_sw_light,//指定控件
						screen_sw_light_event_cb, //指定回调函数
						LV_EVENT_VALUE_CHANGED, //检测控件状态变化
						NULL);
	
	lv_obj_add_event_cb(guider_ui.screen_sw_buzzer,//指定控件
						screen_sw_buzzer_event_cb, //指定回调函数
						LV_EVENT_VALUE_CHANGED, //检测控件状态变化
						NULL);

	for(;;)
  {
	/******************DTH11传感器UI更新*******************/
	 // 检查队列中是否有数据（非阻塞，超时时间0）

	
		if(xQueueReceive(xUI_Datas_Queue, &recv_data, 0)==pdTRUE)//非阻塞
        {
			            // 接收到数据，更新UI
            lv_label_set_text_fmt(guider_ui.screen_humidity, "%d%%", recv_data.humi);
            lv_label_set_text_fmt(guider_ui.screen_temperature, "%d C", recv_data.temp);
        }

		 
		
		lv_timer_handler(); /* 处理LVGL定时器和任务（必须周期性调用） */
		osDelay(5);

  }
  /* USER CODE END StartDefaultTask */
}



/* ******************串口通讯任务************ */
void USART2_Transmit_Task(void *pvParameters)
{

	
	
    UART_Msg_t msg;  // 接收所有类型的消息
    UART_Packet_t tx_packet; //发送数据包

	for(;;)
    {
		
			if(xQueueReceive(xUART2_TxQueue,&msg,pdMS_TO_TICKS(10))==pdTRUE)
			{
				
				switch(msg.type)  
                {
					
					
                   case MSG_TYPE_DHT11:          
						UART_Packet_Init(&tx_packet, 0X01); // 发送湿度
						tx_packet.data[0] = msg.data.dht11_data.humi;
						xQueueSendToBack(xUART2_PacketQueue, &tx_packet, 0);
				   
						UART_Packet_Init(&tx_packet, 0X02); // 发送湿度
						tx_packet.data[0] = msg.data.dht11_data.temp;
						xQueueSendToBack(xUART2_PacketQueue, &tx_packet, 0);
                     break;
				   
					case MSG_TYPE_LED:
						UART_Packet_Init(&tx_packet, 0X03);
						tx_packet.data[0] = msg.data.led_cmd;
						xQueueSendToBack(xUART2_PacketQueue, &tx_packet, 0);
						break;
						
					case MSG_TYPE_BUZZER:
						UART_Packet_Init(&tx_packet, 0X04);
						tx_packet.data[0] = msg.data.buzzer_cmd;
						xQueueSendToBack(xUART2_PacketQueue, &tx_packet, 0);
						break;
				   
				   
                }


			}
			
			if(g_uart2_tx_state == UART_TX_IDLE)	//UASRT是否空闲
			{
				UART_Packet_t tx_packet;
				if(xQueueReceive(xUART2_PacketQueue, &tx_packet, 0) == pdTRUE)
				{
					g_uart2_tx_state = UART_TX_BUSY;//UASRT置位
					HAL_UART_Transmit_DMA(&huart2, (uint8_t*)&tx_packet, sizeof(tx_packet));
				}
			}


			osDelay(2); // 短暂延时，降低CPU占用
    }
	
}


void USART2_Transmit_Task_create(void)
{
	
    
    xUART2_PacketQueue = xQueueCreate(16, sizeof(UART_Packet_t));//发送数据队列
    configASSERT(xUART2_PacketQueue != NULL);
	

	xTaskCreate(USART2_Transmit_Task,
				"USART2_Transmit_Task",
				128*2,
				NULL,
				3,
				&USART2_Transmit_TaskHand_t
				);
}

void UART_Packet_Init(UART_Packet_t *packet, uint8_t type)
{
	packet->header = 0X55;
	packet->tail=0XBB;
	packet->type = type;
	memset(packet->data,0,sizeof(packet->data));
}
