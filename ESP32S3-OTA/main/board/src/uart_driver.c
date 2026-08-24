#include "myuart.h"
#include <stdint.h>
#include "driver/uart.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include "onenet_ota.h"
#include <inttypes.h>

/*============================================================================
 *                           全局变量定义
 *============================================================================*/

static const char *TAG = "uart_driver";
const static size_t buffer_size = 256;

/* 物模型业务帧固定格式：55 | type | data[5] | BB。 */
#define UART_BUSINESS_FRAME_SIZE       8U
#define UART_BUSINESS_HEAD             0x55U
#define UART_BUSINESS_TAIL             0xBBU
#define UART_BUSINESS_ACK_TYPE         0x7EU
#define UART_BUSINESS_ACK_OK           0x00U
#define UART_BUSINESS_ACK_INVALID      0x01U
#define UART_BUSINESS_ACK_QUEUE_FULL   0x02U
#define UART_BUSINESS_TYPE_VERSION     0x05U

/*
 * STM32 当前通讯任务只识别固定 8 字节 ACK：
 * 0x55 0x7E result 0x00 0x00 0x00 0x00 0xBB。
 * 接收端使用固定长度状态机，避免将 UART_DATA 事件大小误判为协议帧长度。
 */
static uint8_t s_business_frame[UART_BUSINESS_FRAME_SIZE];
static size_t s_business_frame_length = 0U;

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
static void uart_business_parser_reset(void);
static void uart_send_business_ack(uart_port_t uart_num, uint8_t result);
static void uart_business_feed_byte(uart_port_t uart_num, uint8_t byte);

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
    xTaskCreatePinnedToCore(uart_recive_buff,
                            "uart_recive_buff",
                            4096,
                            (void*)uart_num,
                            10,
                            &recive_buff1_handle,
                            0);

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

    /* 仅在任务启动时打印一次，用于确认接收任务已被调度运行。 */
    ESP_LOGI(TAG, "[UART][TASK] UART%d receive task started", (int)uart_num);

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
                /* OTA 期间普通业务任务主动放弃本事件，记录原因以免误判为未收到数据。 */
                ESP_LOGW(TAG, "[UART][DROP] OTA busy, event type=%d ignored", (int)uart_ev.type);
                continue;   /* 丢弃事件，回到xQueueReceive继续等下一个 */
            }

            switch (uart_ev.type)
            {
            case UART_DATA:  /* 收到数据事件 */
                /* 先打印事件字节数，再读取数据，可区分“未产生事件”和“读取内容异常”。 */
                ESP_LOGI(TAG, "[UART][RX] UART_DATA event, size=%d", (int)uart_ev.size);

                /*
                 * UART_DATA 的 size 只是当前驱动事件携带的字节数，不能假设它等于一帧。
                 * 因此分块读完本事件的全部字节，再逐字节交给状态机：
                 * - 一帧拆成多个事件：状态机会保留已收字节；
                 * - 一个事件包含多帧：循环会连续解析多次；
                 * - 单个干扰字节：找不到帧头时直接丢弃，不发送无意义 NACK。
                 */
                size_t remain = uart_ev.size;

                while (remain > 0U)
                {
                    size_t request_length = (remain > buffer_size) ? buffer_size : remain;
                    int read_length = uart_read_bytes(uart_num,
                                                      buffer,
                                                      request_length,
                                                      pdMS_TO_TICKS(1000));

                    if (read_length <= 0)
                    {
                        /* 事件已到但底层缓冲区没有读到数据，保留已有半帧，等待下一事件续收。 */
                        ESP_LOGW(TAG, "[UART][RX] read timeout, remain=%u", (unsigned int)remain);
                        break;
                    }

                    //ESP_LOGI(TAG, "[UART][RX][RAW] %d bytes", read_length);
                    //ESP_LOG_BUFFER_HEX(TAG, buffer, read_length);

                    for (int index = 0; index < read_length; index++)
                    {
                        uart_business_feed_byte(uart_num, buffer[index]);
                    }

                    remain -= (size_t)read_length;
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
 * @brief 清空物模型帧解析状态
 * @param 无
 * @return 无
 * @note 只清空软件状态，不读取或清空 ESP-IDF 的 UART 底层接收缓冲区。
 */
static void uart_business_parser_reset(void)
{
    s_business_frame_length = 0U;
}

/**
 * @brief 向 STM32 发送一帧固定格式的物模型应答
 * @param uart_num 要发送的 UART 端口号
 * @param result 应答结果：00成功、01帧或类型错误、02OneNET队列已满
 * @return 无
 * @note ACK 数据位的其余四字节当前保留为 0，后续可扩展序号或错误细节。
 */
static void uart_send_business_ack(uart_port_t uart_num, uint8_t result)
{
    const uint8_t ack_frame[UART_BUSINESS_FRAME_SIZE] = {
        UART_BUSINESS_HEAD,
        UART_BUSINESS_ACK_TYPE,
        result,
        0x00U,
        0x00U,
        0x00U,
        0x00U,
        UART_BUSINESS_TAIL
    };

    /* uart_write_bytes 将数据写入 ESP-IDF 的 TX 缓冲区，因此局部数组在函数返回后安全。 */
    if (uart_write_bytes(uart_num, (const char *)ack_frame, sizeof(ack_frame)) ==
        (int)sizeof(ack_frame))
    {
        //ESP_LOGI(TAG, "[UART][TX][ACK] result=0x%02X", result);
        ESP_LOG_BUFFER_HEX(TAG, ack_frame, sizeof(ack_frame));
    }
    else
    {
        /* 发送长度异常时必须明确记录，避免把“未发出 ACK”误判为 STM32 接收异常。 */
        ESP_LOGW(TAG, "[UART][TX][ACK] write failed, result=0x%02X", result);
    }
}

/**
 * @brief 向物模型解析状态机输入一个 UART 接收字节
 * @param uart_num 当前 UART 端口号，用于完整帧后的 ACK 发送
 * @param byte 本次接收的一个字节
 * @return 无
 * @note 本函数只在 uart_recive_buff 任务上下文调用，不会与 OTA 任务并发解析业务帧。
 */
static void uart_business_feed_byte(uart_port_t uart_num, uint8_t byte)
{
    esp_err_t ret;

    /* 空闲时只寻找帧头，其他字节视为线路噪声或旧数据并直接忽略。 */
    if (s_business_frame_length == 0U)
    {
        if (byte == UART_BUSINESS_HEAD)
        {
            s_business_frame[0] = byte;
            s_business_frame_length = 1U;
        }
        return;
    }

    /* 已找到帧头后，按固定长度依次收集其余 7 个字节。 */
    s_business_frame[s_business_frame_length++] = byte;
    if (s_business_frame_length < UART_BUSINESS_FRAME_SIZE)
    {
        return;
    }

    /* 第 8 字节必须为帧尾；否则本帧无效，并立即重新寻找下一帧头。 */
    if (s_business_frame[UART_BUSINESS_FRAME_SIZE - 1U] != UART_BUSINESS_TAIL)
    {
        ESP_LOGW(TAG, "[UART][BIZ] invalid frame tail");
        uart_send_business_ack(uart_num, UART_BUSINESS_ACK_INVALID);
        uart_business_parser_reset();

        /* 当前错误字节本身若恰好是新帧头，可作为下一帧的第一个字节复用。 */
        if (byte == UART_BUSINESS_HEAD)
        {
            s_business_frame[0] = byte;
            s_business_frame_length = 1U;
        }
        return;
    }

    //ESP_LOGI(TAG, "[UART][RX][BUS] complete frame");
    ESP_LOG_BUFFER_HEX(TAG, s_business_frame, UART_BUSINESS_FRAME_SIZE);

    /* 物模型帧成功入队或版本帧成功缓存后，才回复成功ACK。 */
    ret = uartData_to_oneNET(s_business_frame);
    if (ret == ESP_OK)
    {
        uart_send_business_ack(uart_num, UART_BUSINESS_ACK_OK);
    }
    else if (ret == ESP_ERR_TIMEOUT)
    {
        uart_send_business_ack(uart_num, UART_BUSINESS_ACK_QUEUE_FULL);
    }
    else
    {
        uart_send_business_ack(uart_num, UART_BUSINESS_ACK_INVALID);
    }

    uart_business_parser_reset();
}

/**
 * @brief 物模型数据解析函数
 * @param uart_buffr 串口数据缓冲区
 * @return ESP_OK：已进入 OneNET 队列；ESP_ERR_TIMEOUT：队列已满；
 *         ESP_ERR_NOT_SUPPORTED：type 不支持；ESP_FAIL：帧头或帧尾错误
 * @note 数据包格式：0x55 + 命令(1) + 数据(5) + 0xBB。本函数不等待队列空位。
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
                break;
            
            case 0X02:  /* 温度数据 */
                oneNET_Data.type = 0X02;
                oneNET_Data.value[0] = uart_buffr[2];
                break;
                
            case 0X03:  /* 灯状态 */
                oneNET_Data.type = 0X03;
                oneNET_Data.value[0] = uart_buffr[2];
                break;

            case 0X04:  /* 蜂鸣器状态 */
                oneNET_Data.type = 0X04;
                oneNET_Data.value[0] = uart_buffr[2];
                break;

            case UART_BUSINESS_TYPE_VERSION:  /* STM32 APP当前版本 */
                /* data[3]、data[4]为保留字段，非0说明协议版本不一致。 */
                if ((uart_buffr[5] != 0x00U) || (uart_buffr[6] != 0x00U))
                {
                    ESP_LOGW(TAG, "STM32版本帧保留字段无效");
                    return ESP_ERR_INVALID_ARG;
                }

                if (onenet_ota_notify_stm32_version(uart_buffr[2],
                                                     uart_buffr[3],
                                                     uart_buffr[4]) != ESP_OK)
                {
                    ESP_LOGW(TAG, "STM32版本尚未缓存，等待APP重发");
                    return ESP_ERR_INVALID_STATE;
                }

                /* 版本帧不进入物模型队列；ACK仅表示ESP32已可靠缓存。 */
                return ESP_OK;

            default:
                /* 当前业务协议仅定义0x01~0x05，其他type不应被错误确认为成功。 */
                ESP_LOGW(TAG, "[UART][BIZ] unsupported type=0x%02X", temp);
                return ESP_ERR_NOT_SUPPORTED;
        }

        /*
         * 不能在 UART 接收任务中无限等待队列空位：否则会阻塞后续 ACK 和 OTA 事件处理。
         * 队列已满时返回 ESP_ERR_TIMEOUT，上层转换为 result=02 告知 STM32 重传。
         */
        if ((uart_oneNET_data_queue == NULL) ||
            (xQueueSend(uart_oneNET_data_queue, &oneNET_Data, 0U) != pdPASS))
        {
            ESP_LOGW(TAG, "[UART][BIZ] OneNET queue full");
            return ESP_ERR_TIMEOUT;
        }

        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }
}
