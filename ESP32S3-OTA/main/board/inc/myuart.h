#ifndef _MYUART_H_
#define _MYUART_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "onenet_mqtt.h"
#include "esp_crc.h"

/*============================================================================
 *                      串口固件传输协议定义
 *============================================================================
 * 协议帧格式说明：
 * 
 * 1. 固件信息帧（命令0x05）：
 *    | 包头(1) | 命令(1) | 固件大小(4) | 分隔符(2) | 版本号(8) | 固件CRC32(4) | 帧CRC32(4) | 包尾(1) |
 *    总长度：25字节
 *    说明：固件CRC32 = 对整个固件文件计算的CRC32（端到端校验，保护落盘正确性）；
 *          帧CRC32  = 对"命令+大小+分隔符+版本号+固件CRC32"共19字节计算的CRC32（帧完整性）。
 * 
 * 2. 固件数据帧（命令0x06）：
 *    | 包头(1) | 命令(1) | 包序号(2) | 数据长度(4) | 数据(1024) | CRC32(4) | 包尾(1) |
 *    总长度：1037字节
 * 
 * 3. 简单命令帧（通知/确认/结束）：
 *    | 包头(1) | 命令(1) | 包尾(1) |
 *    总长度：3字节
 * 
 * 传输流程：
 *    ESP32 -> STM32: 升级通知(0x01)
 *    STM32 -> ESP32: ACK应答(0x02)
 *    ESP32 -> STM32: 确认(0x03)
 *    STM32 -> ESP32: ACK应答(0x02)
 *    ESP32 -> STM32: 开始传输(0x04)      <-- ESP32主动通知开始传输
 *    STM32 -> ESP32: ACK应答(0x02)       //此时STM32应先完成檫除，再应答
 *    ESP32 -> STM32: 固件信息(0x05)
 *    STM32 -> ESP32: ACK应答(0x02)
 *    ESP32 -> STM32: 数据包0~N(0x06)
 *    STM32 -> ESP32: ACK应答(0x02) [每包都应答]
 *    ESP32 -> STM32: 升级结束(0x07)
 *    STM32 -> ESP32: ACK应答(0x02) [然后重启]
 *============================================================================*/

/*---------------------------- 协议帧标识 ----------------------------*/
#define SERIAL_PACKET_HEAD          0x66    /* 帧头标识，用于识别数据帧开始 */
#define SERIAL_PACKET_TAIL          0xBB    /* 帧尾标识，用于识别数据帧结束 */
#define SERIAL_PACKET_SEPARATOR     0x2F    /* 分隔符，用于固件信息帧中分隔大小和版本 */

/*---------------------------- 命令码定义 ----------------------------*/
#define SERIAL_CMD_UPGRADE_NOTIFY   0x01    /* 升级通知：ESP32通知STM32准备升级 */
#define SERIAL_CMD_ACK              0x02    /* ACK应答：STM32确认收到数据 */
#define SERIAL_CMD_CONFIRM          0x03    /* 确认命令：ESP32确认握手成功 */
#define SERIAL_CMD_START_TRANS      0x04    /* 开始传输：ESP32通知STM32开始固件传输 */
#define SERIAL_CMD_FW_INFO          0x05    /* 固件信息：包含固件大小和版本号 */
#define SERIAL_CMD_FW_DATA          0x06    /* 固件数据：包含一个数据分片 */
#define SERIAL_CMD_UPGRADE_END      0x07    /* 升级结束：通知STM32重启 */

#define SERIAL_CMD_ABORT            0x08    /* 放弃传输：通知STM32重置回IDLE */
#define SERIAL_CMD_NACK             0x0A    /* STM32否定应答，携带1字节错误码 */

/* NACK错误码（与STM32端TaskTest.h保持一致） */
#define NACK_ERR_FRAME_CRC          0x01    /* 帧CRC失败(可重发恢复) */
#define NACK_ERR_STATE_MISMATCH     0x02    /* 状态不同步：ABORT对齐后外层重试 */
#define NACK_ERR_BAD_LENGTH         0x03    /* 帧长非法(可重发恢复) */
#define NACK_ERR_FLASH_ERROR        0x04    /* Flash写失败：硬件故障，不可恢复 */
#define NACK_ERR_FW_CRC_FAIL        0x05     /* 固件整体CRC失败：数据损坏，不可恢复 */
#define NACK_ERR_UNKNOWN_CMD        0x06    /* 未知命令：通常为噪声扰动，可恢复 */
#define NACK_ERR_PACKET_SEQ         0x07    /* 包序号异常，可恢复 */


/*---------------------------- 传输参数配置 ----------------------------*/
#define SERIAL_DATA_CHUNK_SIZE      1024    /* 每个数据包的数据长度，1KB */
#define SERIAL_MAX_RETRY_COUNT      5       /* 最大重试次数，超过则判定传输失败 */
#define SERIAL_TIMEOUT_MS           8000    /* 普通命令超时时间，8秒,STM32檫除时间较长 */
#define SERIAL_HANDSHAKE_TIMEOUT_MS 10000   /* 握手等待超时时间，10秒（给STM32准备时间） */

/*---------------------------- 数据包大小计算 ----------------------------*/
#define SERIAL_FW_INFO_PACKET_SIZE  25      /* 固件信息帧总长度：1+1+4+2+8+4+4+1=25字节 */
#define SERIAL_FW_DATA_HEADER_SIZE  8       /* 数据帧头部长度：包头+命令+序号+长度 = 1+1+2+4=8字节 */
#define SERIAL_FW_DATA_TAIL_SIZE    5       /* 数据帧尾部长度：CRC32+包尾 = 4+1=5字节 */
#define SERIAL_FW_DATA_PACKET_SIZE  (SERIAL_FW_DATA_HEADER_SIZE + SERIAL_DATA_CHUNK_SIZE + SERIAL_FW_DATA_TAIL_SIZE)
                                            /* 数据帧总长度：8+1024+5=1037字节 */

/*============================================================================
 *                           数据结构定义
 *============================================================================*/

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
    uint8_t type;       /* 数据类别：0x01湿度/0x02温度/0x03灯/0x04蜂鸣器 */
    uint8_t value[3];   /* 数据值：最多3字节 */
} uart_oneNET_data_t;

extern QueueHandle_t uart_oneNET_data_queue;   /* 串口数据传递oneNET队列 */
extern volatile uint8_t g_last_nack_err;   /* 最近一次收到的NACK错误码，0=无 */
extern volatile bool g_ota_uart_busy;      /* OTA传输期间串口归OTA任务独占（标志门控，替代vTaskSuspend） */
/*============================================================================
 *                           函数声明
 *============================================================================*/

/**
 * @brief 串口初始化和创建串口接收任务
 * @param uart_param 串口参数配置结构体指针
 * @param uart_num 串口号
 */
void UART_config_init_AND_UART_recive_TASK(uart_param_config_t *uart_param,
                                        uart_port_t uart_num);

/**
 * @brief 计算CRC32校验值（一次性，对整块数据）
 * @param data 待计算的数据指针
 * @param len 数据长度
 * @return CRC32校验值
 * @note 标准CRC-32：多项式0xEDB88320，初值0xFFFFFFFF，最终异或0xFFFFFFFF，与STM32端OTA_CalcCRC32兼容
 */
uint32_t calculate_crc32(const uint8_t *data, size_t len);

/**
 * @brief 流式CRC32：用于对分块下载/读取的数据累加计算整体CRC32
 *        三步用法：crc = crc32_stream_init(); crc = crc32_stream_update(crc, chunk, n); ... crc = crc32_stream_final(crc);
 *        结果与 calculate_crc32() 对同一份数据完全一致。
 */
uint32_t crc32_stream_init(void);                                              /* 返回初值 0xFFFFFFFF */
uint32_t crc32_stream_update(uint32_t crc, const uint8_t *data, size_t len);   /* 累加一段数据，不做最终异或 */
uint32_t crc32_stream_final(uint32_t crc);                                     /* 做最终异或，返回最终CRC32 */

/**
 * @brief 发送ABORT命令对齐状态或直接放弃上报
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 用于在传输失败时，对齐状态或直接放弃上报
 */
esp_err_t send_abort(uart_port_t uart_num);


/**
 * @brief 带重试机制的数据包发送函数
 * @param uart_num 串口号
 * @param data 待发送数据指针
 * @param len 数据长度
 * @param expected_cmd 期望收到的应答命令码
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 采用停止等待协议，发送后等待ACK，超时则重试
 */
esp_err_t send_packet_with_retry(uart_port_t uart_num, const uint8_t *data, size_t len, uint8_t expected_cmd);

/**
 * @brief 设置固件信息（在传输前调用）
 * @param firmware_size 固件大小（字节）
 * @param version 固件版本号字符串
 * @param fw_crc 整个固件文件的CRC32校验值（端到端校验用）
 */
void set_firmware_info(int firmware_size, const char* version, uint32_t fw_crc);

/**
 * @brief 通过串口发送固件到STM32
 * @param uart_num 串口号
 * @param firmware_path 固件文件路径（SPIFFS文件系统中的路径）
 * @param stm32_in_bootloader 是否在STM32引导模式下（true/false）
 * @note 用于判断是否需要发送ABORT命令对齐状态或直接放弃上报
 * @note 传输前会先设置固件信息，传输后会发送END命令
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 完整的固件传输流程，包括握手、数据传输、结束确认
 */
esp_err_t serial_send_firmware(uart_port_t uart_num, const char* firmware_path, bool stm32_in_bootloader);


#endif
