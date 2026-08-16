#ifndef USART1_BOOT_H
#define USART1_BOOT_H

#include "main.h"
#include "usart.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>


#define Bootloader_Handle huart1	//串口1句柄
#define U1RX_Bufffer_Size   2048	//接收缓冲区大小
#define U1TX_Bufffer_Size   2048    // 发送缓冲区大小
#define U1RX_Data_Max      256				//单次DMA空闲中断接收的字节大小
#define URXData_SE_Size  10		//接收指针数组长度（最多记录10次接收数据的起始地址）


	

typedef struct  
{
	uint8_t *start;
	uint8_t *end;
}UCB_URxBuffptr;	//定义负责记录接收和结束数据的指针结构体



typedef struct  
{
	uint16_t  U1RX_Counter;		//计算接收缓冲区已使用的总字节数
	UCB_URxBuffptr URxDataBuffer[URXData_SE_Size];	//起止指针结构体数组创建
	UCB_URxBuffptr *URxDataIn;							// 入队指针（记录新接收的数据）
	UCB_URxBuffptr *URxDataOut;							// 出队指针（处理已接收的数据）
	UCB_URxBuffptr *URxDataEnd;							// 指针数组的末尾边界
}UCB_ControlBlock;





void UCBRx_PtrInit(void);
void U1BootLoader_Init(void);
void U1_Printf(char *format, ...);
void U1RXBootLoader_IDLE_CallBack(UART_HandleTypeDef *huart);
void test_IDLE_UART1(void);




#endif


