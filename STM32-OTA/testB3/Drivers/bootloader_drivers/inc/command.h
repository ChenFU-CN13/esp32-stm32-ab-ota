#ifndef COMMAND_H
#define COMMAND_H

#include "main.h"
#include "usart.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

//移植时需要修改的参数
#define UART_HANDLE       (&huart2)
#define UART_RX_BUFFER_SIZE  8192U      //环形缓冲区大小
#define UART_RX_BUFFER_MASK  (UART_RX_BUFFER_SIZE - 1U)
#define UART_DMA_RX_MAX      2048U      //单次DMA接收最大长度

/* 使用累计式环形缓冲区，需要限制软件环容量必须是非零的2的幂。 */
#if ((UART_RX_BUFFER_SIZE == 0U) || \
     ((UART_RX_BUFFER_SIZE & UART_RX_BUFFER_MASK) != 0U))
#error "UART_RX_BUFFER_SIZE必须为非零的2的幂"
#endif

/* 现有读取接口使用uint16_t返回长度，因此容量不能超出它的范围。 */
#if (UART_RX_BUFFER_SIZE > 65535U)
#error "UART_RX_BUFFER_SIZE超出uint16_t长度接口的范围"
#endif


//接口函数
void     UART_circular_buffer_Init(void);
uint16_t UART_circular_buffer_GetRxLen(void);
uint16_t UART_circular_buffer_ReadData(uint8_t *buf, uint16_t len);
void UART_IDLE_CallBack(UART_HandleTypeDef *huart);//注意:要在中断函数中调用,否则会导致死

void test(void);

/**
 * @brief 新增：获取因软件环空间不足而拒收的字节数量。
 * @param 无。
 * @return 自初始化以来的软件环拒收字节数，允许自然回绕。
 * @note 不包含UART硬件错误或DMA停机期间丢失的数据。
 */
uint32_t UART_circular_buffer_GetOverflowCount(void);
/* 以下接口只在Bootloader主循环使用，不能与其他读取者同时操作软件环。 */
uint16_t UART_circular_buffer_PeekData(uint8_t *buf, uint16_t len); /* 查看数据，不推进读计数。 */
void UART_circular_buffer_Clear(void); /* 丢弃当前已进入软件环的数据。 */
#endif


