#ifndef COMMAND_H
#define COMMAND_H

#include "main.h"
#include "usart.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

//移植时需要修改的参数
#define UART_HANDLE       &huart2
#define UART_RX_BUFFER_SIZE 8192
#define UART_DMA_RX_MAX      2048

//接口函数
void     UART_circular_buffer_Init(void);
uint16_t UART_circular_buffer_GetRxLen(void);
uint16_t UART_circular_buffer_ReadData(uint8_t *buf, uint16_t len);
void UART_IDLE_CallBack(UART_HandleTypeDef *huart);//注意:要在中断函数中调用,否则会导致死
void test(void);
#endif


