
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "gui_guider.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdio.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "DTH11.h"
#include "buzzer.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

typedef struct {
    uint8_t temp;
    uint8_t humi;
} sensor_Data_t;		//添加新的传感器数据到这个结构体，蜂鸣器 LED不属于

// LED控制指令枚举
typedef enum {
    LED_CMD_OFF = 0,  // 关闭
    LED_CMD_ON,       // 打开
} LED_Cmd_t;

// 报警器控制指令枚举
typedef enum {
    buzzer_CMD_OFF = 0,  // 关闭
    buzzer_CMD_ON,       // 打开
} buzzer_Cmd_t;


typedef struct {
    uint8_t header;     // 固定为0x55
    uint8_t type;       // 数据类型
    uint8_t data[5];    // 数据位
    uint8_t tail;       // 固定为0xBB
}UART_Packet_t;			//数据帧格式规范


// 新增：串口消息类型枚举
typedef enum {
    MSG_TYPE_DHT11,
    MSG_TYPE_LED,
    MSG_TYPE_BUZZER
} UART_MsgType_t;

// 新增：统一的串口消息结构体
typedef struct {
    UART_MsgType_t type;	//数据判断解耦，避免交叉
    union {					//联合体
        sensor_Data_t dht11_data;
        LED_Cmd_t led_cmd;
        buzzer_Cmd_t buzzer_cmd;
    } data;
} UART_Msg_t;


//// 串口消息类型枚举（明确区分所有可能的消息），另外一种方案解耦数据交叉
//typedef enum {
//    UART_MSG_DHT11,    // DHT11温湿度数据
//    UART_MSG_LED_CMD,  // LED控制指令
//    UART_MSG_BUZZER_CMD // 蜂鸣器控制指令
//} UART_MsgType_t;

//// 扁平化串口消息结构体（所有字段平铺）
//typedef struct {
//    UART_MsgType_t type;  // 消息类型（标识当前有效字段）
//    // 所有可能用到的字段直接定义（根据type判断有效性）
//    uint8_t temp;         // 温度（仅UART_MSG_DHT11有效）
//    uint8_t humi;         // 湿度（仅UART_MSG_DHT11有效）
//    LED_Cmd_t led_cmd;    // LED指令（仅UART_MSG_LED_CMD有效）
//    buzzer_Cmd_t buzzer_cmd; // 蜂鸣器指令（仅UART_MSG_BUZZER_CMD有效）
//} UART_Msg_t;





extern sensor_Data_t g_dht11_data;	//串口2发送数据
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */




/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN Handle_t */

extern QueueHandle_t xsensor_DataQueue;// DHT11队列句柄（全局可见）
extern QueueHandle_t xLEDQueue;// LED控制队列句柄（全局可见）
extern QueueHandle_t xbuzzerQueue;// LED控制队列句柄（全局可见）
extern QueueHandle_t xUI_Datas_Queue;// 数据队列句柄（全局可见）

extern QueueHandle_t xUART2_TxQueue;	//串口数据中转队列（存储UART_Msg_t）

extern SemaphoreHandle_t xDHT11Mutex;//互斥量句柄（全局可见）


/* USER CODE END Handle_t */

/* Exported functions prototypes ---------------------------------------------*/


/* USER CODE BEGIN EFP */

void AppTasks_Create(void);// 任务创建函数

void screen_sw_light_event_cb(lv_event_t *e);// 灯事件回调函数

void screen_sw_buzzer_event_cb(lv_event_t *e);// 报警器事件回调函数




/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif 
