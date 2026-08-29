#ifndef _UART_DRIVER_H_
#define _UART_DRIVER_H_

#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/*串口参数配置初始化*/
typedef struct
{
    int baud_rate;              /* 波特率 */
    uart_word_length_t data_bits;   /* 数据位：5/6/7/8位 */
    uart_parity_t parity;           /* 奇偶校验：无/奇/偶 */
    uart_stop_bits_t stop_bits;     /* 停止位：1/1.5/2位 */
    uart_port_t UART_PORT;          /* 串口号：UART_NUM_0/1/2 */
    int GPIO_TX;                    /* TX引脚GPIO编号 */
    int GPIO_RX;                    /* RX引脚GPIO编号 */
} uart_param_config_t;

/*oneNET获取数据更新结构体*/
typedef struct
{
    uint8_t type;       /* 0x01湿度/0x02温度/0x03灯/0x04报警/0x06烟雾/0x07光照/0x08风扇 */
    uint8_t value[3];   /* 数据值：最多3字节 */
} uart_oneNET_data_t;

extern QueueHandle_t uart_queue;            /* UART事件队列句柄 */
extern QueueHandle_t uart_oneNET_data_queue;   /* 串口数据传递oneNET队列 */
extern volatile bool g_ota_uart_busy;      /* OTA传输期间串口归OTA任务独占（标志门控，替代vTaskSuspend） */

/**
 * @brief 串口初始化和创建串口接收任务
 * @param uart_param 串口参数配置结构体指针
 * @param uart_num 串口号
 */
void UART_config_init_AND_UART_recive_TASK(uart_param_config_t *uart_param,
                                        uart_port_t uart_num);

/**
 * @brief 向 STM32 发送 OneNET 本地连接心跳。
 * @param online true=OneNET在线，false=离线。
 * @return ESP_OK=完整写入8字节；ESP_ERR_INVALID_STATE=UART未初始化或OTA独占；ESP_FAIL=写入失败。
 * @note 帧格式为 55 0A status 00 00 00 00 BB，不进入 OneNET 属性上报。
 */
esp_err_t uart_send_business_heartbeat(bool online);

#endif
