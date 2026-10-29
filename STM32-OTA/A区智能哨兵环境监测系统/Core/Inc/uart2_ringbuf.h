#ifndef __UART2_RINGBUF_H
#define __UART2_RINGBUF_H

#include <stdint.h>

/* USART2 接收缓冲区：仅允许中断写入、通讯任务读取。 */
#define UART2_RINGBUF_SIZE       64U                    //缓冲区长度
#define UART2_RINGBUF_MASK (UART2_RINGBUF_SIZE - 1U)    //最后一位的下标
/**
 * @brief 初始化 USART2 接收环形缓冲区。
 * @param 无。
 * @return 无。
 * @note 系统启动阶段、开启 USART2 单字节中断接收前调用一次。
 */
void UART2_RingBuffer_Init(void);

/**
 * @brief 将 USART2 接收的一个字节写入环形缓冲区。
 * @param byte USART2 本次接收到的字节。
 * @return 1：成功写入；0：缓冲区已满，已丢弃该新字节。
 * @note 仅允许在 USART2 接收中断/回调上下文调用；它是唯一写入者。
 */
uint8_t UART2_RingBuffer_PushFromISR(uint8_t byte);

/**
 * @brief 从环形缓冲区读取一个最早到达的字节。
 * @param byte 用于存放读出字节的地址，不能为空。
 * @return 1：成功读出；0：缓冲区为空或参数为空。
 * @note 仅允许 UART2_Comm_Task 调用；它是唯一读取者。
 */
uint8_t UART2_RingBuffer_Pop(uint8_t *byte);

/**
 * @brief 获取因缓冲区写满而丢弃的新字节总数。
 * @param 无。
 * @return 溢出累计次数。
 * @note 通讯任务发现该数值变化后，会放弃当前半帧并重新寻找帧头。
 */
uint32_t UART2_RingBuffer_GetOverflowCount(void);

#endif
