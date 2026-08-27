/* Includes ------------------------------------------------------------------*/
#include "APP_Tasks.h"

//*********** 全局共享数据  ********//
void test_Packet_Init(UART_Packet_t *packet, uint8_t type);
QueueHandle_t xUART2_TxQueue;

/************************* DHT11读取任务变量声明 *************************/

SemaphoreHandle_t xDHT11Mutex=NULL;	
static TaskHandle_t xDHT11TaskHandle = NULL;// 任务句柄（静态存储，用于动态创建）
xQueueHandle xUI_Datas_Queue;//UI数据队列
/************************* LED控制任务变量声明 *************************/
// LED控制队列（长度3，存储LED指令）
QueueHandle_t xLEDQueue;
static TaskHandle_t xlightTaskHandle = NULL;

/************************* 报警器任务变量声明 *************************/
QueueHandle_t xbuzzerQueue;
static TaskHandle_t xbuzzerTaskHandle = NULL;





/************************* DHT11读取任务 *************************/

//感觉可以用一个结构体变量来传数据，一个队列

void DHT11Task(void *pvParameters)
{
    uint8_t temp, humi=0;

	sensor_Data_t xsensor_DTH11QueueData = {0};//初始化清0
	UART_Msg_t msg;  // 新增：用于发送DHT11数据到串口任务


    if(dht11_init() != 0)
    {
        printf("DHT11 init failed!\r\n");
    }
	else
	{
		printf("DHT11 init OK!\r\n");
	}
	
	
	
	 for(;;)
    {
		if(dht11_read_data(&temp,&humi) == 0)//读数据
		{
			if(xSemaphoreTake(xDHT11Mutex,portMAX_DELAY))//申请互斥锁
            {
				
				
				// 填充队列数据结构体
                xsensor_DTH11QueueData.temp = temp;
                xsensor_DTH11QueueData.humi = humi;
				
				// 发送到UI队列
				if(xQueueSendToBack(xUI_Datas_Queue, &xsensor_DTH11QueueData, 0)!= pdPASS)
				{
					
					printf("UI_Queue is full, data not sent!\r\n");// 队列已满时的错误处理（可选）
				}
				
				
				
				//发送到串口队列
				msg.type = MSG_TYPE_DHT11;
				msg.data.dht11_data = xsensor_DTH11QueueData;
				// 发送到串口队列
				if(xQueueSendToBack(xUART2_TxQueue, &msg, 0)!= pdPASS)
				{
					
					printf("Serial_Queue is full, data not sent!\r\n");// 队列已满时的错误处理（可选）
				}
				
				xSemaphoreGive(xDHT11Mutex);// 释放互斥锁
            }

		}
		osDelay(2000);// 2秒读取一次
    }
	
	
}

/************************* 报警灯任务 *************************/
void lightTask(void *pvParameters)
{
	LED_Cmd_t led_cmd; // 直接使用APP_Tasks.h中定义的类型	
	led_init();
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET); // 初始灭
    UART_Msg_t msg;  // 使用新的消息结构体

	 for(;;)
    {

		
		// 接收LED指令（队列中存储的是LED_Cmd_t类型）
		if(xQueueReceive(xLEDQueue, &led_cmd, pdMS_TO_TICKS(500)))
		{
			
			switch(led_cmd)
            {
                case LED_CMD_ON:
                    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
					msg.type = MSG_TYPE_LED;
					msg.data.led_cmd = LED_CMD_ON;
					xQueueSendToBack(xUART2_TxQueue,&msg,0);
                    break;
					
					
                case LED_CMD_OFF:
                    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
					msg.type = MSG_TYPE_LED;
					msg.data.led_cmd = LED_CMD_OFF;
					xQueueSendToBack(xUART2_TxQueue,&msg,0);
                    break;
                default:
                    break;
            }
		}
		
		
    }
	
	
}

/************************* 报警器任务 *************************/
void buzzerTask(void *pvParameters)
{
	buzzer_Cmd_t buzzer_Cmd; // 直接使用APP_Tasks.h中定义的类型
	beep_init();
    UART_Msg_t msg;  // 使用新的消息结构体
	 for(;;)
    {
		
        if(xQueueReceive(xbuzzerQueue, &buzzer_Cmd, pdMS_TO_TICKS(500)))
        {
            switch(buzzer_Cmd)
            {
				
                case buzzer_CMD_ON:
					HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_SET);
					msg.type = MSG_TYPE_BUZZER;
					msg.data.buzzer_cmd = buzzer_CMD_ON;
					xQueueSendToBack(xUART2_TxQueue,&msg,0);
                    break;
					
					
                case buzzer_CMD_OFF:
					HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET);
					msg.type = MSG_TYPE_BUZZER;
					msg.data.buzzer_cmd = buzzer_CMD_OFF;
					xQueueSendToBack(xUART2_TxQueue,&msg,0);
                    break;
					
					
                default:
                    break;
            }
			
			
        }
		
    }
	
	
}


/************************* LVGL开关回调（若在该文件实现） *************************/
/**
 * @brief       LED开关回调
 * @param       事件对象
 * @retval      无
 */
void screen_sw_light_event_cb(lv_event_t *e) //读取开关当前是开还是关，把这个状态发给 LED 任务
{
    lv_obj_t *switch_obj = lv_event_get_target(e);			//获取控件对象
    bool is_on = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);	//获取控件返回的状态
    
    LED_Cmd_t led_cmd = is_on ? LED_CMD_ON : LED_CMD_OFF; // 使用定义的枚举

    xQueueSendToBack(xLEDQueue, &led_cmd, 0); // 发送指令到队列
}


/**
 * @brief       报警器开关回调
 * @param       事件对象
 * @retval      无
 */
void screen_sw_buzzer_event_cb(lv_event_t *e)
{
	lv_obj_t *switch_obj = lv_event_get_target(e);
	bool is_on = lv_obj_has_state(switch_obj,LV_STATE_CHECKED);
	buzzer_Cmd_t buzzer_Cmd = is_on ? buzzer_CMD_ON : buzzer_CMD_OFF;
	xQueueSendToBack(xbuzzerQueue, &buzzer_Cmd, 0); // 发送指令到队列
}

// 新增：UI 交互状态全局变量（test4 新增控件）
uint8_t g_fan_pwm = 0;		// 风扇占空比 0~100%
uint8_t g_alarm_light_sw = 0;	// 报警灯开关状态（0=关 1=开）

/**
 * @brief       风扇调速进度条回调（test4 新增控件）
 * @param       事件对象
 * @retval      无
 */
void screen_bar_fan_event_cb(lv_event_t *e)
{
	lv_obj_t *bar = lv_event_get_target(e);	// 获取进度条控件
	uint16_t val = lv_bar_get_value(bar);		// 读取当前值 0~1000
	g_fan_pwm = val / 10;						// 换算成 0~100%
	lv_label_set_text_fmt(guider_ui.screen_fan_num, "%d%%", g_fan_pwm); // 同步显示在风扇数值上
	// TODO: 风扇驱动逻辑，后续在此将 g_fan_pwm 通过 UART2 发送到 ESP32（物模型 0x08 风扇指令）
}

/**
 * @brief       报警灯开关回调（test4 新增控件）
 * @param       事件对象
 * @retval      无
 */
void screen_sw_Alarm_light_event_cb(lv_event_t *e)
{
	lv_obj_t *switch_obj = lv_event_get_target(e);
	g_alarm_light_sw = lv_obj_has_state(switch_obj, LV_STATE_CHECKED) ? 1 : 0;
	// TODO: 报警灯驱动逻辑，后续由报警引擎（AlarmEngineTask）读取 g_alarm_light_sw 联动
}


void test_Packet_Init(UART_Packet_t *packet, uint8_t type)
{
	packet->header = 0X55;
	packet->tail=0XBB;
	packet->type = type;
	memset(packet->data,0,sizeof(packet->data));
}

/************************* 任务创建入口函数 *************************/
void AppTasks_Create(void)
{
	//创建互斥锁
	xDHT11Mutex = xSemaphoreCreateMutex();
	configASSERT(xDHT11Mutex != NULL);//确保成功
	
	
	// 原消息队列（将UART_Msg_t 数据传输到 串口通讯任务）
    xUART2_TxQueue = xQueueCreate(16, sizeof(UART_Msg_t));
    configASSERT(xUART2_TxQueue != NULL);
	
	
	xUI_Datas_Queue = xQueueCreate(5,sizeof(sensor_Data_t));
	configASSERT(xUI_Datas_Queue != NULL);

	// 初始化LED控制队列（新增）
    xLEDQueue = xQueueCreate(3, sizeof(LED_Cmd_t));
    configASSERT(xLEDQueue != NULL);
	
	//初始化buzzer队列
	xbuzzerQueue = xQueueCreate(3, sizeof(buzzer_Cmd_t));
	configASSERT(xbuzzerQueue != NULL);
	
	xTaskCreate(DHT11Task,		// 任务函数
				"DHT11Task",	//任务名称
				128*4,			//任务栈大小
				NULL,			//任务参数
				6,				//任务优先级
				&xDHT11TaskHandle  //任务句柄
				);
	configASSERT(xDHT11TaskHandle != NULL);//确保成功
	
	

	xTaskCreate(lightTask,		// 任务函数
				"lightTask",	//任务名称
				128/2,			//任务栈大小
				NULL,			//任务参数
				5,				//任务优先级
				&xlightTaskHandle  //任务句柄
				);
	configASSERT(xlightTaskHandle != NULL);//确保成功
	
	
	xTaskCreate(buzzerTask,		// 任务函数
				"buzzerTask",	//任务名称
				128/2,			//任务栈大小
				NULL,			//任务参数
				5,				//任务优先级
				&xbuzzerTaskHandle  //任务句柄
				);
	configASSERT(xbuzzerTaskHandle != NULL);//确保成功
	

	
}

/* USER CODE END 3 */
