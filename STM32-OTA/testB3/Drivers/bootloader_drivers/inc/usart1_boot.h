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
#define U1RX_Data_Max       256		//单次DMA空闲中断接收的字节大小
#define URXData_SE_Size     10		//接收指针数组长度（最多记录10次接收数据的起始地址）


	

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
//知识回顾：UCB_URxBuffptr *代表指向 UCB_URxBuffptr 这个类型的结构体变量
/*假如说有创建一个 UCB_URxBuffptr a 和 UCB_URxBuffptr *p;
p = &a;//这里就是相当于把这个结构体变量的地址值告诉了p
a就相当于一个大房子，&a相当于房子的门牌号，p知道了门牌号之后，就可以送快递给这个a房子

而UCB_URxBuffptr URxDataBuffer[URXData_SE_Size]; 就相当于创建了10个箱子来装门牌号

URxDataIn和URxDataOut，就相当于控制使用这十个箱子的工具


用*/



void UCBRx_PtrInit(void);
void U1BootLoader_Init(void);
void U1_Printf(char *format, ...);
void U1RXBootLoader_IDLE_CallBack(UART_HandleTypeDef *huart);
void test_IDLE_UART1(void);

//外部可用
extern uint8_t U1_RX_Buffer[U1RX_Bufffer_Size];
extern UCB_ControlBlock UCB_CB;

#endif


