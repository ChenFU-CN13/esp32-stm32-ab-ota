#include "myuart.h"
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "driver/uart.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include <inttypes.h>

/*============================================================================
 *                           全局变量定义
 *============================================================================*/

static const char *TAG = "myuart";
const static size_t buffer_size = 256;
const static uint8_t uart_recive_ack[2] = {0xFE, 0xFF};  /* 发送"接收应答"给STM32 */

QueueHandle_t uart_queue = NULL;              /* UART事件队列句柄 */
QueueHandle_t uart_oneNET_data_queue = NULL;  /* 串口数据传递到oneNET的队列 */
TaskHandle_t recive_buff1_handle = NULL;      /* 串口接收任务句柄 */

/* 固件传输相关全局变量 */
static int g_firmware_size = 0;               /* 固件大小（字节） */
static char g_firmware_version[16] = {0};     /* 固件版本号字符串 */
static uint32_t g_firmware_crc32 = 0;        /* 整个固件文件的CRC32（端到端校验用，由下载时流式计算得到） */
volatile uint8_t g_last_nack_err = 0;       /* NACK错误标志位 */
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

/*============================================================================
 *                       固件传输协议实现函数
 *============================================================================*/

/**
 * @brief 计算标准CRC32校验值（一次性，对整块数据）
 * @param data 待计算的数据指针
 * @param len 数据长度
 * @return CRC32校验值（32位无符号整数）
 *
 * @note 标准CRC-32算法参数：
 *       - 多项式: 0xEDB88320 (反向表示)
 *       - 初始值: 0xFFFFFFFF
 *       - 最终异或: 0xFFFFFFFF
 *       - 与STM32端OTA_CalcCRC32兼容
 *       逐帧CRC与固件整体CRC使用同一算法，保证可比性。
 */
uint32_t calculate_crc32(const uint8_t *data, size_t len)
{
    return crc32_stream_final(crc32_stream_update(crc32_stream_init(), data, len));
}

/**
 * @brief 流式CRC32初值
 * @return 0xFFFFFFFF（标准CRC-32初值）
 */
uint32_t crc32_stream_init(void)
{
    return 0xFFFFFFFFU;
}

/**
 * @brief 流式CRC32累加一段数据（不做最终异或，可跨chunk连续调用）
 * @param crc 上一次的累加值（首次用 crc32_stream_init() 的返回值）
 * @param data 本段数据指针
 * @param len 本段数据长度
 * @return 累加后的中间值（未做最终异或）
 */
uint32_t crc32_stream_update(uint32_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;  /* 反向多项式 */
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 流式CRC32收尾：做最终异或，得到最终CRC32
 * @param crc crc32_stream_update() 的最后返回值
 * @return 最终CRC32校验值
 */
uint32_t crc32_stream_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFU;  /* 最终异或 */
}

/**
 * @brief 带重试机制的数据包发送函数
 * @param uart_num 串口号
 * @param data 待发送数据指针
 * @param len 数据长度
 * @param expected_cmd 期望收到的应答命令码
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 该函数实现了停止等待协议（Stop-and-Wait）：
 *       1. 发送数据包
 *       2. 等待接收方返回ACK
 *       3. 如果超时未收到正确ACK，则重发
 *       4. 重复步骤1-3，直到成功或达到最大重试次数
 *       
 *       这种协议虽然效率较低，但实现简单且可靠性高，
 *       适合固件传输这种对可靠性要求极高的场景。
 */
//esp_err_t send_packet_with_retry(uart_port_t uart_num, const uint8_t *data, size_t len, uint8_t expected_cmd)
//{
//    uint8_t recv_buffer[16] = {0};
//    int retry_count = 0;
//    
//    /* 重试循环：最多重试SERIAL_MAX_RETRY_COUNT次 */
//    while (retry_count < SERIAL_MAX_RETRY_COUNT)
//    {
//        /* 清空接收缓冲区，避免上次残留数据干扰 */
//        uart_flush_input(uart_num);
//        
//        /* 发送数据包 */
//        uart_write_bytes(uart_num, data, len);
//        ESP_LOGI(TAG, "Sent packet, waiting for ACK (cmd=0x%02X), retry=%d", expected_cmd, retry_count);
//        
//        /* 等待接收ACK，超时时间SERIAL_TIMEOUT_MS */
//        int recv_len = uart_read_bytes(uart_num, recv_buffer, 3, pdMS_TO_TICKS(SERIAL_TIMEOUT_MS));
//        
//        /* 验证接收到的ACK：
//         * - 长度必须为3字节
//         * - 第1字节必须是包头0x66
//         * - 第2字节必须是期望的命令码
//         * - 第3字节必须是包尾0xBB
//         */
//        if (recv_len == 3 && 
//            recv_buffer[0] == SERIAL_PACKET_HEAD && 
//            recv_buffer[1] == expected_cmd && 
//            recv_buffer[2] == SERIAL_PACKET_TAIL)
//        {
//            ESP_LOGI(TAG, "Received expected ACK");
//            return ESP_OK;
//        }
//        
//        /* 未收到正确ACK，准备重试 */
//        ESP_LOGW(TAG, "Invalid response or timeout, retry %d/%d", retry_count + 1, SERIAL_MAX_RETRY_COUNT);
//        retry_count++;
//    }
//    
//    /* 达到最大重试次数仍未成功 */
//    ESP_LOGE(TAG, "Max retry count reached, failed to get ACK");
//    return ESP_FAIL;
//}


esp_err_t send_packet_with_retry(uart_port_t uart_num, const uint8_t *data, size_t len, uint8_t expected_cmd)
{
    uint8_t recv_buffer[8] = {0};
    int retry_count = 0;

     g_last_nack_err = 0;    /* 每次进入先清除上次错误码 */


    while (retry_count < SERIAL_MAX_RETRY_COUNT) 
    {
        /* 清空接收缓冲区，避免上次残留数据干扰 */
        uart_flush_input(uart_num);

        /* 发送数据包 */
        uart_write_bytes(uart_num,data,len);
        ESP_LOGI(TAG, "Sent packet, waiting for ACK (cmd=0x%02X), retry=%d", expected_cmd, retry_count);

        /* 先读3字节：ACK是3字节(66 02 BB)，NACK前3字节是(66 0A err)，读3字节就可以辨认出是正常ACK，还是NACK了 */
        int recv_len  = uart_read_bytes(uart_num,recv_buffer,3,pdMS_TO_TICKS(SERIAL_TIMEOUT_MS));

        /* 情况1：收到期望的ACK，成功 */
        if (recv_len == 3 && 
            recv_buffer[0] == SERIAL_PACKET_HEAD &&
            recv_buffer[1] == expected_cmd &&
            recv_buffer[2] == SERIAL_PACKET_TAIL)
        {
            ESP_LOGI(TAG,"Received expected ACK");
            return ESP_OK; // 成功直接返回！
        }


        /* 情况2：收到NACK，读第4字节(包尾)确认并提取错误码 */
        if (recv_len == 3 && 
            recv_buffer[0] == SERIAL_PACKET_HEAD &&
            recv_buffer[1] == SERIAL_CMD_NACK 
            )
        {
            uint8_t err_code = recv_buffer[2];
            int n = uart_read_bytes(uart_num,&recv_buffer[3],1,pdMS_TO_TICKS(200));

            if (n == 1 && recv_buffer[3] == SERIAL_PACKET_TAIL)
            {
                g_last_nack_err = err_code;
                ESP_LOGE(TAG, "Received NACK, err_code=0x%02X", err_code);

                /* 可恢复错误(传输干扰)：重发同一帧可能恢复，继续重试 */
                if (err_code == NACK_ERR_FRAME_CRC ||
                    err_code == NACK_ERR_BAD_LENGTH ||
                    err_code == NACK_ERR_PACKET_SEQ
                    ) 
                {
                    retry_count++;
                    continue;   /* 走正常重试 */


                }

                /* 不可恢复错误(状态错位/Flash/固件问题)：重发无意义，立即失败
                 * 上层会发ABORT对齐状态或直接放弃上报 */
                return ESP_FAIL;


            }


        }


        /* 情况3：超时或垃圾数据：可能丢包，重试 */
        ESP_LOGW(TAG, "Invalid response or timeout, retry %d/%d", retry_count + 1, SERIAL_MAX_RETRY_COUNT);
        retry_count++;






    }

    ESP_LOGE(TAG, "Max retry count reached, failed to get ACK");
    return ESP_FAIL;



}





/**
 * @brief 发送ABORT命令，通知STM32放弃本次传输并重置回IDLE
 * @note 用在两种场景：
 *       1. 传输彻底失败后，对齐双端状态再退出
 *       2. 不可恢复NACK(状态不匹配)后，拉回IDLE准备整体重试
 */
esp_err_t send_abort(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_ABORT, SERIAL_PACKET_TAIL};
    uint8_t saved_nack = g_last_nack_err;   /* 保存，防止被本次调用清掉 */
    esp_err_t ret;
    ESP_LOGW(TAG, "Sending ABORT command...");
    ret = send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
    g_last_nack_err = saved_nack;           /* 恢复，让上层日志打印真实错误码 */
    return ret;
}

















/**
 * @brief 发送升级通知帧
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x01 0xBB（包头 + 升级通知命令 + 包尾）
 */
esp_err_t send_handshake_notify(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_UPGRADE_NOTIFY, SERIAL_PACKET_TAIL};
    return send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
}

/**
 * @brief 发送握手确认帧
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x03 0xBB（包头 + 确认命令 + 包尾）
 */
esp_err_t send_handshake_confirm(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_CONFIRM, SERIAL_PACKET_TAIL};
    return send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
}

/**
 * @brief 发送固件信息帧
 * @param uart_num 串口号
 * @param firmware_size 固件大小（字节）
 * @param version 固件版本号字符串
 * @param fw_crc 整个固件文件的CRC32（端到端校验值）
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 数据包格式（共25字节）：
 *       | 字节偏移 | 内容          | 说明                            |
 *       |----------|---------------|--------------------------------|
 *       | 0        | 0x66          | 包头                            |
 *       | 1        | 0x05          | 固件信息命令                     |
 *       | 2-5      | firmware_size | 固件大小（大端序，4字节）         |
 *       | 6-7      | 0x2F 0x2F     | 分隔符                          |
 *       | 8-15     | version       | 版本号（8字节，不足补0）         |
 *       | 16-19    | fw_crc        | 固件整体CRC32（大端序，4字节）   |
 *       | 20-23    | frame_crc     | 帧CRC32（大端序，4字节）         |
 *       | 24       | 0xBB          | 包尾                            |
 *       
 *       固件整体CRC32：对整个固件文件计算，STM32端收完数据后用它做端到端校验。
 *       帧CRC32：对 packet[1]~packet[19]（命令+大小+分隔符+版本号+固件CRC32，共19字节）计算，保证帧自身完整。
 *       
 *       大端序说明：高字节在前，低字节在后
 *       例如：固件大小0x00012345，存储为：00 01 23 45
 */
esp_err_t send_firmware_info(uart_port_t uart_num, int firmware_size, const char* version, uint32_t fw_crc)
{
    uint8_t packet[SERIAL_FW_INFO_PACKET_SIZE];
    int pos = 0;
    
    /* 构建数据包头部 */
    packet[pos++] = SERIAL_PACKET_HEAD;      /* 包头 */
    packet[pos++] = SERIAL_CMD_FW_INFO;      /* 命令码 */
    
    /* 固件大小：4字节，大端序存储
     * 大端序：高位字节存储在低地址
     * >> 24: 取最高字节
     * >> 16: 取次高字节
     * >> 8:  取次低字节
     * & 0xFF: 取最低字节
     */
    packet[pos++] = (firmware_size >> 24) & 0xFF;  /* 最高字节 */
    packet[pos++] = (firmware_size >> 16) & 0xFF;
    packet[pos++] = (firmware_size >> 8) & 0xFF;
    packet[pos++] = firmware_size & 0xFF;          /* 最低字节 */
    
    /* 分隔符：用于区分固件大小和版本号 */
    packet[pos++] = SERIAL_PACKET_SEPARATOR;
    packet[pos++] = SERIAL_PACKET_SEPARATOR;
    
    /* 版本号：固定8字节
     * 例如版本号"v1.0.2"存储为：'v' '1' '.' '0' '.' '2' 0x00 0x00 0x00
     * 版本号"v1.0"存储为：'v' '1' '.' '0' 0x00 0x00 0x00 0x00
     * 不足8字节时后面填充0x00
     */
    int version_len = strlen(version);
    for (int i = 0; i < 8; i++)
    {
        if (i < version_len)
        {
            packet[pos++] = version[i];
        }
        else
        {
            packet[pos++] = 0x00;  /* 填充0x00 */
        }
    }

    /* 固件整体CRC32：4字节，大端序存储
     * 这是整个固件文件的CRC32，STM32端收完数据后用它做端到端校验
     */
    packet[pos++] = (fw_crc >> 24) & 0xFF;
    packet[pos++] = (fw_crc >> 16) & 0xFF;
    packet[pos++] = (fw_crc >> 8) & 0xFF;
    packet[pos++] = fw_crc & 0xFF;
    
    /* 帧CRC32校验值
     * 校验范围：从命令码开始到固件CRC32结束（不包含包头和帧CRC32本身）
     * 即：packet[1] 到 packet[19]，共19字节
     */
    uint32_t frame_crc = calculate_crc32(&packet[1], pos - 1);
    
    /* 帧CRC32：4字节，大端序存储 */
    packet[pos++] = (frame_crc >> 24) & 0xFF;
    packet[pos++] = (frame_crc >> 16) & 0xFF;
    packet[pos++] = (frame_crc >> 8) & 0xFF;
    packet[pos++] = frame_crc & 0xFF;
    
    /* 包尾 */
    packet[pos++] = SERIAL_PACKET_TAIL;
    
    ESP_LOGI(TAG, "Sending firmware info: size=%d, version=%s, fw_crc=0x%08" PRIx32 ", frame_crc=0x%08" PRIx32,
             firmware_size, version, fw_crc, frame_crc);
    
    return send_packet_with_retry(uart_num, packet, pos, SERIAL_CMD_ACK);
}

/**
 * @brief 发送固件数据包
 * @param uart_num 串口号
 * @param packet_seq 数据包序号（0~N-1）
 * @param data 数据指针（1024字节）
 * @param data_len 数据长度
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 数据包格式（共1037字节）：
 *       | 字节偏移   | 内容        | 说明                      | CRC校验 |
 *       |------------|-------------|--------------------------|---------|
 *       | 0          | 0x66        | 包头                      | 不参与  |
 *       | 1          | 0x06        | 固件数据命令               | 不参与  |
 *       | 2-3        | packet_seq  | 包序号（大端序，2字节）     | 不参与  |
 *       | 4-7        | data_len    | 数据长度（大端序，4字节）   | 不参与  |
 *       | 8-1031     | data        | 固件数据（1024字节）       | 参与    |
 *       | 1032-1035  | CRC32       | 校验值（大端序，4字节）    | 不参与  |
 *       | 1036       | 0xBB        | 包尾                      | 不参与  |
 *       
 *       CRC32校验说明：
 *       - 只对固件数据部分（1024字节）计算CRC
 *       - 命令字、包序号、数据长度由协议保证正确性，无需校验
 *       - 使用标准CRC-32算法，与STM32硬件CRC兼容
 *       
 *       包序号的作用：
 *       - 用于检测丢包：如果STM32收到的序号不连续，说明有丢包
 *       - 用于检测乱序：如果STM32收到的序号顺序不对，说明数据乱序
 *       - 支持断点续传：记录最后成功接收的序号，可以从该位置继续
 */
esp_err_t send_firmware_data_packet(uart_port_t uart_num, uint16_t packet_seq, const uint8_t* data, size_t data_len)
{
    /* 动态分配数据包缓冲区 */
    uint8_t* packet = malloc(SERIAL_FW_DATA_PACKET_SIZE);
    if (packet == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate packet buffer");
        return ESP_FAIL;
    }
    
    int pos = 0;
    
    /* 构建数据包头部 */
    packet[pos++] = SERIAL_PACKET_HEAD;      /* 包头 */
    packet[pos++] = SERIAL_CMD_FW_DATA;      /* 命令码 */
    
    /* 包序号：2字节，大端序存储 */
    packet[pos++] = (packet_seq >> 8) & 0xFF;   /* 高字节 */
    packet[pos++] = packet_seq & 0xFF;          /* 低字节 */
    
    /* 数据长度：4字节，大端序存储 */
    uint32_t len_val = data_len;
    packet[pos++] = (len_val >> 24) & 0xFF;
    packet[pos++] = (len_val >> 16) & 0xFF;
    packet[pos++] = (len_val >> 8) & 0xFF;
    packet[pos++] = len_val & 0xFF;
    
    /* 复制固件数据 */
    memcpy(&packet[pos], data, data_len);
    pos += SERIAL_DATA_CHUNK_SIZE;
    
    /* 计算CRC32校验值 - 只校验固件数据部分（1024字节）
     * 命令字、包序号、数据长度由协议保证正确性，无需校验
     */
    uint32_t crc = calculate_crc32(data, data_len);
    
    /* CRC32：4字节，大端序存储 */
    packet[pos++] = (crc >> 24) & 0xFF;
    packet[pos++] = (crc >> 16) & 0xFF;
    packet[pos++] = (crc >> 8) & 0xFF;
    packet[pos++] = crc & 0xFF;
    
    /* 包尾 */
    packet[pos++] = SERIAL_PACKET_TAIL;
    
    ESP_LOGI(TAG, "Sending data packet: seq=%d, len=%d, CRC=0x%08" PRIx32, packet_seq, data_len, crc);
    
    /* 发送数据包并等待ACK */
    esp_err_t ret = send_packet_with_retry(uart_num, packet, pos, SERIAL_CMD_ACK);
    
    /* 释放缓冲区 */
    free(packet);
    
    
    return ret;
}

/**
 * @brief 发送升级结束帧
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x07 0xBB（包头 + 升级结束命令 + 包尾）
 *       STM32收到此命令后会重启并运行新固件
 */
esp_err_t send_upgrade_end(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_UPGRADE_END, SERIAL_PACKET_TAIL};
    return send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
}

/**
 * @brief 发送固件开始命令
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x04 0xBB（包头 + 开始传输命令 + 包尾）
 *       该命令通知STM32即将开始发送固件数据，STM32完成檫除后才会应答
 */
esp_err_t send_start_transmission(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_START_TRANS, SERIAL_PACKET_TAIL};
    return send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
}

/**
 * @brief 通过串口发送固件到STM32（主函数）
 * @param uart_num 串口号
 * @param firmware_path 固件文件路径（SPIFFS文件系统中的路径）
 * @param stm32_in_bootloader 是否在STM32引导模式下（true/false）
 * @note 用于判断是否需要发送ABORT命令对齐状态或直接放弃上报
 * @note 传输前会先设置固件信息，传输后会发送END命令
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 完整的固件传输流程：
 *       
 *       阶段1：握手阶段
 *       -----------------
 *       Step 1: ESP32发送升级通知 -> STM32返回ACK
 *       Step 2: ESP32发送握手确认 -> STM32返回ACK
 *       Step 3: ESP32发送开始传输命令 -> STM32返回ACK
 *       
 *       阶段2：信息交换
 *       -----------------
 *       Step 4: ESP32发送固件信息（大小+版本） -> STM32返回ACK
 *       
 *       阶段3：数据传输
 *       -----------------
 *       Step 5: 循环发送固件数据包
 *               - 每包1024字节
 *               - 每包都需要STM32返回ACK
 *               - 显示传输进度
 *       
 *       阶段4：结束阶段
 *       -----------------
 *       Step 6: ESP32发送升级结束 -> STM32返回ACK并重启
 */
esp_err_t serial_send_firmware(uart_port_t uart_num, const char* firmware_path, bool stm32_in_bootloader)
{
    ESP_LOGI(TAG, "Starting firmware transmission via serial");

    /*===== 旧方案（已废弃，注释保留对比）=====
    vTaskSuspend(recive_buff1_handle);
    ESP_LOGI(TAG, "Suspended UART receive task");
     * 死因：挂起时接收任务可能正持有UART驱动内部的rx_mux互斥锁
     * （阻塞在uart_read_bytes里消化积压事件），被冻结后锁永不释放，
     * 本任务后面调uart_flush_input()要无限期等这把锁 → Step 2永久卡死。
     * vTaskSuspend的致命缺陷：你无法控制任务被冻结在代码的哪一行，
     * 冻在持锁区间就是死锁。
     *=======================================*/

    /* 新方案：标志门控。接收任务继续活着跑事件循环，但OTA期间
     * 只消费事件不碰ringbuffer，既不抢数据也不抢锁 */
    g_ota_uart_busy = true;
    ESP_LOGI(TAG, "UART locked by OTA task (flag gate)");
    
    esp_err_t ret = ESP_OK;
    int fd = -1;                    /* 文件描述符，用于读取固件文件 */
    uint8_t* data_buffer = NULL;    /* 数据缓冲区，用于存储每次读取的1KB数据 */
    
    /*==================== 打开固件文件 ====================*/
    /* 以只读方式打开固件文件 */
    fd = open(firmware_path, O_RDONLY);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Failed to open firmware file: %s", firmware_path);
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    /* 获取文件大小：lseek移动到文件末尾，返回偏移量即为文件大小 */
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);  /* 移回文件开头，准备读取 */
    
    if (file_size <= 0)
    {
        ESP_LOGE(TAG, "Invalid firmware file size: %d", (int)file_size);
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "Firmware file size: %d bytes", (int)file_size);
    
    /* 分配数据缓冲区，用于每次读取1KB固件数据 */
    data_buffer = malloc(SERIAL_DATA_CHUNK_SIZE);
    if (data_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate data buffer");
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    /*==================== 阶段1：握手阶段 ====================*/
    
    /* Step 1: 发送升级通知
     * 目的：通知STM32即将进行固件升级，让STM32进入升级准备状态
     * 数据包：0x66 0x01 0xBB
     */
    if (!stm32_in_bootloader)
    {
        ESP_LOGI(TAG, "Step 1: Sending upgrade notification...");
        ret = send_handshake_notify(uart_num);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Handshake notify failed");
            goto cleanup;
        }
        ESP_LOGI(TAG, "Step 1: STM32 acknowledged upgrade notification");
    }
    else
    {
        ESP_LOGI(TAG, "Skip Step 1: STM32 already in bootloader (retry)");
    }

    
    /* Step 2: 发送握手确认
     * 目的：确认握手成功，建立通信链路
     * 数据包：0x66 0x03 0xBB
     */
    ESP_LOGI(TAG, "Step 2: Sending handshake confirm...");
    ret = send_handshake_confirm(uart_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Handshake confirm failed");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 2: Handshake confirmed");
    
    /* Step 3: 发送开始传输命令
     * 目的：通知STM32即将开始发送固件数据
     *       STM32收到此命令后会先擦除Flash，擦除完成后才返回ACK
     * 数据包：0x66 0x04 0xBB
     * 注意：由于Flash擦除需要较长时间，此处超时设置为8秒
     */
    ESP_LOGI(TAG, "Step 3: Sending start transmission command...");
    ret = send_start_transmission(uart_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Start transmission failed");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 3: STM32 ready for firmware transmission");
    
    /*==================== 阶段2：信息交换 ====================*/
    
    /* Step 4: 发送固件信息（大小+版本号）
     * 目的：告知STM32即将接收的固件大小和版本号
     *       STM32根据固件大小分配Flash空间
     * 数据包格式：包头(1) + 命令(1) + 固件大小(4) + 分隔符(2) + 版本号(4) + CRC32(4) + 包尾(1)
     * 总长度：17字节
     */
    ESP_LOGI(TAG, "Step 4: Sending firmware info...");
    ret = send_firmware_info(uart_num, (int)file_size, g_firmware_version, g_firmware_crc32);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send firmware info");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 4: Firmware info sent successfully");
    
    /*==================== 阶段3：数据传输 ====================*/
    
    /* Step 5: 循环发送固件数据包 */
    ESP_LOGI(TAG, "Step 5: Starting firmware data transmission...");
    
    /* 计算总包数：(文件大小 + 每包大小 - 1) / 每包大小
     * 这种计算方式可以正确处理文件大小不是每包大小整数倍的情况
     * 例如：文件大小2049字节，每包1024字节
     * (2049 + 1024 - 1) / 1024 = 3072 / 1024 = 3包
     */
    uint16_t total_packets = (file_size + SERIAL_DATA_CHUNK_SIZE - 1) / SERIAL_DATA_CHUNK_SIZE;
    uint16_t current_packet = 0;  /* 当前包序号，从0开始 */
    size_t total_sent = 0;        /* 已发送的总字节数 */
    
    /* 循环读取并发送数据包，直到所有数据发送完毕 */
    while (total_sent < file_size)
    {
        /* 从文件读取一块数据（最多1024字节） */
        size_t read_len = read(fd, data_buffer, SERIAL_DATA_CHUNK_SIZE);
        if (read_len <= 0)
        {
            ESP_LOGE(TAG, "Failed to read firmware data at offset %d", total_sent);
            ret = ESP_FAIL;
            goto cleanup;
        }
        
        /* 如果读取的数据不足1024字节（通常是最后一包），用0xFF填充
         * STM32根据固件大小字段知道实际有效数据长度，会忽略填充数据
         */
        if (read_len < SERIAL_DATA_CHUNK_SIZE)
        {
            memset(data_buffer + read_len, 0xFF, SERIAL_DATA_CHUNK_SIZE - read_len);
        }
        
        /* 发送数据包并等待ACK */
        ret = send_firmware_data_packet(uart_num, current_packet, data_buffer, SERIAL_DATA_CHUNK_SIZE);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send packet %d", current_packet);
            goto cleanup;
        }
        
        /* 更新进度计数器 */
        current_packet++;
        total_sent += read_len;
        
        /* 打印传输进度，方便调试和监控 */
        ESP_LOGI(TAG, "Progress: %d/%d packets (%d/%d bytes, %.1f%%)", 
                 current_packet, total_packets, total_sent, (int)file_size,
                 (float)total_sent * 100 / file_size);
    }
    
    ESP_LOGI(TAG, "Step 5: Firmware data transmission completed");
    
    /*==================== 阶段4：结束阶段 ====================*/
    
    /* Step 6: 发送升级结束命令
     * 目的：通知STM32固件传输完成，STM32将重启并运行新固件
     * 数据包：0x66 0x07 0xBB
     * STM32收到后会验证固件完整性，然后重启
     */
    ESP_LOGI(TAG, "Step 6: Sending upgrade end command...");
    ret = send_upgrade_end(uart_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send upgrade end");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 6: Upgrade end acknowledged, STM32 will restart");
    
    ESP_LOGI(TAG, "=== Firmware transmission completed successfully! ===");
    
    /*==================== 清理资源 ====================*/
cleanup:
    /* 关闭文件描述符 */
    if (fd >= 0)
    {
        close(fd);
    }
    
    /* 释放数据缓冲区内存 */
    if (data_buffer != NULL)
    {
        free(data_buffer);
    }
    /* 传输失败退出前，先发ABORT让STM32重置回IDLE，保证下次重试时双端状态对齐 */
    if (ret != ESP_OK)
    {
        send_abort(uart_num);
    }

    /*===== 旧方案（已废弃，注释保留对比）=====
    vTaskResume(recive_buff1_handle);
    ESP_LOGI(TAG, "Resumed UART receive task");
     * 问题：任务恢复后要消化挂起期间积压的过期UART_DATA事件，
     * 拿过期事件读空ringbuffer会持锁阻塞1秒，还可能解析出垃圾
     * 数据误发0xFF重发请求给STM32。
     *=======================================*/

    /* 新方案：解锁三件套
     * 1. 清标志：让接收任务恢复正常处理（顺序必须在send_abort之后，
     *    否则ABORT的ACK会被接收任务抢走）
     * 2. uart_flush_input：清掉传输期间可能残留的字节
     * 3. xQueueReset：丢弃积压的过期事件，防止接收任务醒来后
     *    拿陈旧事件去读空缓冲区、误发0xFF给STM32
     */
    g_ota_uart_busy = false;
    uart_flush_input(uart_num);
    xQueueReset(uart_queue);
    ESP_LOGI(TAG, "UART unlocked, stale events flushed");
    
    /* 打印最终结果 */
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Firmware transmission failed!");
    }
    
    return ret;
}







/**
 * @brief 设置固件信息（在传输前调用）
 * @param firmware_size 固件大小（字节）
 * @param version 固件版本号字符串
 * @param fw_crc 整个固件文件的CRC32（端到端校验值）
 * @note 该函数将固件信息保存到全局变量，供后续传输使用
 */
void set_firmware_info(int firmware_size, const char* version, uint32_t fw_crc)
{
    g_firmware_size = firmware_size;
    g_firmware_crc32 = fw_crc;
    if (version != NULL)
    {
        /* 安全复制版本号字符串，防止缓冲区溢出 */
        strncpy(g_firmware_version, version, sizeof(g_firmware_version) - 1);
        g_firmware_version[sizeof(g_firmware_version) - 1] = '\0';  /* 确保字符串以NULL结尾 */
    }
}
