#ifndef _OTA_PROTO_H_
#define _OTA_PROTO_H_

#include "esp_err.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stddef.h>

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
#define SERIAL_CMD_VERSION_REQUEST  0x09    /* 版本查询：请求运行中的STM32 APP重新上报当前版本 */
#define SERIAL_CMD_NACK             0x0A    /* STM32否定应答，携带1字节错误码 */

/* NACK错误码（与STM32端TaskTest.h保持一致） */
#define NACK_ERR_FRAME_CRC          0x01    /* 帧CRC失败(可重发恢复) */
#define NACK_ERR_STATE_MISMATCH     0x02    /* 状态不同步：ABORT对齐后外层重试 */
#define NACK_ERR_BAD_LENGTH         0x03    /* 帧长非法(可重发恢复) */
#define NACK_ERR_FLASH_ERROR        0x04    /* Flash写失败：硬件故障，不可恢复 */
#define NACK_ERR_FW_CRC_FAIL        0x05     /* 固件整体CRC失败：数据损坏，不可恢复 */
#define NACK_ERR_UNKNOWN_CMD        0x06    /* 未知命令：通常为噪声扰动，可恢复 */
#define NACK_ERR_PACKET_SEQ         0x07    /* 包序号异常，可恢复 */
#define NACK_ERR_PREVIOUS_ROLLBACK  0x08    /* STM32 已恢复上一版本，本轮旧会话必须结束 */


/*---------------------------- 传输参数配置 ----------------------------*/
#define SERIAL_DATA_CHUNK_SIZE      1024    /* 每个数据包的数据长度，1KB */
#define SERIAL_MAX_RETRY_COUNT      5       /* 最大重试次数，超过则判定传输失败 */
#define SERIAL_TIMEOUT_MS           8000    /* 普通命令超时时间，8秒,STM32檫除时间较长 */
#define SERIAL_HANDSHAKE_BOOT_DELAY_MS 500U /* APP回ACK后，给STM32复位并启动USART2 DMA的时间 */
#define SERIAL_HANDSHAKE_TIMEOUT_MS  500U   /* 0x03单次等待：必须小于Bootloader的5s握手窗口 */
#define SERIAL_HANDSHAKE_MAX_RETRY   8U     /* 最多等待4s，丢失首帧时仍可在窗口内恢复 */
#define SERIAL_START_TRANS_TIMEOUT_MS 20000U /* 0x04 等待备份与擦除完成 */
#define SERIAL_START_TRANS_MAX_RETRY 1U      /* 一次20秒已覆盖备份+擦除，掉电不重复等待 */
#define SERIAL_ABORT_TIMEOUT_MS      500U    /* 收尾命令有界等待，避免掉电时阻塞40秒 */
#define SERIAL_ABORT_MAX_RETRY       2U

/*---------------------------- 数据包大小计算 ----------------------------*/
#define SERIAL_FW_INFO_PACKET_SIZE  25      /* 固件信息帧总长度：1+1+4+2+8+4+4+1=25字节 */
#define SERIAL_FW_DATA_HEADER_SIZE  8       /* 数据帧头部长度：包头+命令+序号+长度 = 1+1+2+4=8字节 */
#define SERIAL_FW_DATA_TAIL_SIZE    5       /* 数据帧尾部长度：CRC32+包尾 = 4+1=5字节 */
#define SERIAL_FW_DATA_PACKET_SIZE  (SERIAL_FW_DATA_HEADER_SIZE + SERIAL_DATA_CHUNK_SIZE + SERIAL_FW_DATA_TAIL_SIZE)
                                            /* 数据帧总长度：8+1024+5=1037字节 */

extern volatile uint8_t g_last_nack_err;   /* 最近一次收到的NACK错误码，0=无 */
extern volatile uint8_t g_start_transmission_timeout; /* 0x04等待无响应，供上层结束旧会话 */

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
 * @brief 发送ABORT命令对齐状态或直接放弃上报
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 用于在传输失败时，对齐状态或直接放弃上报；收尾等待有界
 */
esp_err_t send_abort(uart_port_t uart_num);

/**
 * @brief 请求运行中的STM32 APP重新上报当前版本
 * @param uart_num 串口号
 * @return ESP_OK表示3字节查询帧已写入UART发送缓冲区，其他值表示参数、状态或发送失败
 * @note 本命令不等待0x66 ACK；STM32以0x55/0x05版本业务帧作为响应，仍由普通UART接收任务解析。
 */
esp_err_t send_version_request(uart_port_t uart_num);

/**
 * @brief 发送升级通知帧（0x01）
 */
esp_err_t send_handshake_notify(uart_port_t uart_num);

/**
 * @brief 发送握手确认帧（0x03）
 */
esp_err_t send_handshake_confirm(uart_port_t uart_num);

/**
 * @brief 发送固件信息帧（0x05，25字节）
 */
esp_err_t send_firmware_info(uart_port_t uart_num, int firmware_size, const char* version, uint32_t fw_crc);

/**
 * @brief 发送固件数据包（0x06，变长）
 */
esp_err_t send_firmware_data_packet(uart_port_t uart_num, uint16_t packet_seq, const uint8_t* data, size_t data_len);

/**
 * @brief 发送升级结束帧（0x07）
 */
esp_err_t send_upgrade_end(uart_port_t uart_num);

/**
 * @brief 发送开始传输命令（0x04）
 */
esp_err_t send_start_transmission(uart_port_t uart_num);

#endif
