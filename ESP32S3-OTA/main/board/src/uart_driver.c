#include "myuart.h"
#include <stdint.h>
#include "driver/uart.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include <inttypes.h>

/*============================================================================
 *                           全局变量定义
 *============================================================================*/

static const char *TAG = "uart_driver";
const static size_t buffer_size = 256;
const static uint8_t uart_recive_ack[2] = {0xFE, 0xFF};  /* 发送"接收应答"给STM32 */

QueueHandle_t uart_queue = NULL;              /* UART事件队列句柄 */
QueueHandle_t uart_oneNET_data_queue = NULL;  /* 串口数据传递到oneNET的队列 */
TaskHandle_t recive_buff1_handle = NULL;      /* 串口接收任务句柄 */

volatile bool g_ota_uart_busy = false;      /* OTA独占串口标志：true期间接收任务只消费事件不碰ringbuffer
                                             * 替代vTaskSuspend方案——挂起任务时它可能正持有驱动内部
                                             * 的rx_mux互斥锁（阻塞在uart_read_bytes里），被冻结后锁
                                             * 永远不释放，OTA任务调uart_flush_input(portMAX_DELAY等锁)
                                             * 就永久死锁（已在IDF 5.x驱动源码中实锤） */


/*============================================================================
 *                           函数前置声明
 *============================================================================*/

esp_err_t uartData_to_oneNET(uint8_t* uart_buffr);
void uart_recive_buff(void* pvParameters);

/*============================================================================
 *                           串口基础功能函数
 *============================================================================*/

/**
 * @brief 串口初始化和创建串口接收任务
 * @param uart_param 串口参数配置结构体指针
 * @param uart_num 串口号
 */
void UART_config_init_AND_UART_recive_TASK(uart_param_config_t *uart_param,
                                        uart_port_t uart_num)
{
    /* 配置UART参数 */
    uart_config_t uart_config = 
    {
        .baud_rate = uart_param->baud_rate,
        .data_bits = uart_param->data_bits,
        .parity = uart_param->parity,
        .stop_bits = uart_param->stop_bits,
        .source_clk = UART_SCLK_DEFAULT
    };
    
    /* 应用UART配置 */
    uart_param_config(uart_param->UART_PORT, &uart_config);
    
    /* 设置UART引脚 */
    uart_set_pin(uart_param->UART_PORT, uart_param->GPIO_TX, uart_param->GPIO_RX, -1, -1);
    
    /* 安装UART驱动程序
     * 参数说明：
     * - UART_PORT: 串口号
     * - 1024: RX缓冲区大小
     * - 1024: TX缓冲区大小
     * - 10: 事件队列大小
     * - &uart_queue: 事件队列句柄（底层自动创建）
     * - 0: 中断分配标志
     */
    uart_driver_install(uart_param->UART_PORT, 1024, 1024, 10, &uart_queue, 0);
    
    /* 创建串口接收任务 */
    xTaskCreate(uart_recive_buff, "uart_recive_buff", 4096, (void*)uart_num, 10, &recive_buff1_handle);

    /* 创建数据队列，用于将串口数据传递到oneNET任务 */
    uart_oneNET_data_queue = xQueueCreate(10, sizeof(uart_oneNET_data_t));
}

/**
 * @brief 串口接收物模型数据任务
 * @param pvParameters 串口号（转换为指针传递）
 * @note 该任务持续运行，处理UART事件队列中的事件
 */
void uart_recive_buff(void* pvParameters)
{
    esp_err_t ret;
    uart_event_t uart_ev;

    /* 分配接收缓冲区 */
    uint8_t* buffer = (uint8_t*)malloc(buffer_size);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        vTaskDelete(NULL);
        return;
    }
    
    /* 获取串口号参数 */
    uart_port_t uart_num = (uart_port_t)pvParameters;

    /* 主循环：持续等待并处理UART事件 */
    for (;;)
    {
        /* 从UART事件队列中获取事件，阻塞等待 */
        if (xQueueReceive(uart_queue, &uart_ev, portMAX_DELAY))
        {
            /*========== OTA独占期门控（新增，替代vTaskSuspend方案） ==========
             * OTA传输期间，ACK/NACK字节由OTA任务直接uart_read_bytes读取，
             * 本任务绝不能碰ringbuffer（会把OTA的应答字节抢走），也不能
             * uart_flush_input（会和OTA任务抢rx_mux锁）。
             * 只把事件消费掉防止队列积压即可。
             * 注意：事件必须实时消费——如果像旧方案那样挂起任务让事件
             * 堆在队列里，恢复后会拿过期事件去读空的ringbuffer，正是
             * 上次死锁的导火索。
             */
            if (g_ota_uart_busy &&
                (uart_ev.type == UART_DATA ||
                 uart_ev.type == UART_BUFFER_FULL ||
                 uart_ev.type == UART_FIFO_OVF))
            {
                continue;   /* 丢弃事件，回到xQueueReceive继续等下一个 */
            }

            switch (uart_ev.type)
            {
            case UART_DATA:  /* 收到数据事件 */
                /* 清空缓冲区，避免旧数据干扰 */
                memset(buffer, 0, buffer_size);
                
                if (uart_ev.size > buffer_size)
                {
                    /* 数据超过缓冲区大小，截断并警告 */
                    ESP_LOGW(TAG, "Data truncated: %d > %d bytes", uart_ev.size, buffer_size);                  
                }
                else
                {
                    /* 读取UART数据 */
                    uart_read_bytes(uart_num, buffer, uart_ev.size, pdMS_TO_TICKS(1000));
                    ESP_LOG_BUFFER_HEX(TAG, buffer, uart_ev.size);

                    /* 检查数据包长度是否足够（物模型协议要求至少8字节） */
                    if (uart_ev.size < 8) {
                        ESP_LOGE(TAG, "Data packet too short: %d bytes", uart_ev.size);
                        uart_write_bytes(uart_num, &uart_recive_ack[1], sizeof(uart_recive_ack[1]));  /* 发送重发请求 */
                        break;
                    }
                    else
                    {
                        /* 解析数据包 */
                        ret = uartData_to_oneNET(buffer);
                        if (ret == ESP_FAIL)
                        {
                            ESP_LOGE(TAG, "Encountered an unexpected error");
                            uart_write_bytes(uart_num, &uart_recive_ack[1], sizeof(uart_recive_ack[1]));  /* 发送重发请求 */
                        }                                  
                        else                                  
                        {
                            /* 数据合法并解析成功，返回应答 */
                            uart_write_bytes(uart_num, &uart_recive_ack[0], sizeof(uart_recive_ack[0]));
                        }
                    }
                }              
                break;
            
            case UART_BUFFER_FULL:  /* RX缓冲区满事件 */
                uart_flush_input(uart_num);  /* 清空RX缓冲区 */
                xQueueReset(uart_queue);     /* 重置事件队列 */
                break;
                
            case UART_FIFO_OVF:  /* UART FIFO溢出事件 */
                uart_flush_input(uart_num);  /* 清空RX缓冲区 */
                xQueueReset(uart_queue);     /* 重置事件队列 */
                break;
                
            default:
                break;
            }
        }
    }
}

/**
 * @brief 物模型数据解析函数
 * @param uart_buffr 串口数据缓冲区
 * @return ESP_OK解析成功，ESP_FAIL解析失败
 * @note 数据包格式：0x55 + 命令(1) + 数据(5) + 0xBB
 */
esp_err_t uartData_to_oneNET(uint8_t* uart_buffr)
{   
    /* 验证数据包头尾 */
    if (uart_buffr[0] == 0x55 && uart_buffr[7] == 0xbb)    
    {
        uint8_t temp = uart_buffr[1];
        uart_oneNET_data_t oneNET_Data;
        
        switch (temp)
        {        
            case 0X01:  /* 湿度数据 */
                oneNET_Data.type = 0X01;
                oneNET_Data.value[0] = uart_buffr[2];
                xQueueSend(uart_oneNET_data_queue, &oneNET_Data, portMAX_DELAY);
                break;
            
            case 0X02:  /* 温度数据 */
                oneNET_Data.type = 0X02;
                oneNET_Data.value[0] = uart_buffr[2];
                xQueueSend(uart_oneNET_data_queue, &oneNET_Data, portMAX_DELAY);
                break;
                
            case 0X03:  /* 灯状态 */
                oneNET_Data.type = 0X03;
                oneNET_Data.value[0] = uart_buffr[2];
                xQueueSend(uart_oneNET_data_queue, &oneNET_Data, portMAX_DELAY);
                break;

            case 0X04:  /* 蜂鸣器状态 */
                oneNET_Data.type = 0X04;
                oneNET_Data.value[0] = uart_buffr[2];
                xQueueSend(uart_oneNET_data_queue, &oneNET_Data, portMAX_DELAY);
                break;

            default:
                break;
        }
        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }
}
