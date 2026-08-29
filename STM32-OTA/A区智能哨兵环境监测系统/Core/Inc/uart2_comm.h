#ifndef __UART2_COMM_H
#define __UART2_COMM_H

#include <stdint.h>

/* 物模型ACK帧：0x55 0x7E result 0x00 0x00 0x00 0x00 0xBB。 */
#define UART2_COMM_BUSINESS_HEAD       0x55U
#define UART2_COMM_BUSINESS_TAIL       0xBBU
#define UART2_COMM_BUSINESS_ACK_TYPE   0x7EU
#define UART2_COMM_HEARTBEAT_TYPE      0x0AU

/* OTA通知和ACK帧：0x66 command 0xBB。 */
#define UART2_COMM_OTA_HEAD            0x66U
#define UART2_COMM_OTA_NOTIFY_CMD      0x01U
#define UART2_COMM_OTA_ACK_CMD         0x02U
#define UART2_COMM_VERSION_REQUEST_CMD 0x09U
#define UART2_COMM_OTA_TAIL            0xBBU

/**
 * @brief 初始化 UART2 通讯任务、内部队列和单字节中断接收。
 * @param 无。
 * @return 无。
 * @note 应在原有业务消息队列 xUART2_TxQueue 创建完成后调用。
 */
void UART2_Comm_Start(void);

/**
 * @brief 从 USART2 接收中断转交一个字节给通讯模块。
 * @param byte 本次接收完成的字节。
 * @return 无。
 * @note 仅限 ISR/回调调用；函数不做完整帧解析。
 */
void UART2_Comm_RxByteFromISR(uint8_t byte);

/**
 * @brief 从 USART2 DMA 发送完成回调通知通讯模块。
 * @param 无。
 * @return 无。
 * @note 仅限 ISR/回调调用；不会在这里复位系统。
 */
void UART2_Comm_TxCompleteFromISR(void);

/**
 * @brief 从 USART2 错误回调通知通讯模块放弃当前半帧。
 * @param 无。
 * @return 无。
 * @note 仅限 ISR/回调调用；实际解析状态由通讯任务安全地重置。
 */
void UART2_Comm_RxErrorFromISR(void);

#endif
