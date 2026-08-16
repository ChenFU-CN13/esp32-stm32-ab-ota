#include "usart1_boot.h"



//缓冲区数组 最大2048
uint8_t U1_RX_Buffer[U1RX_Bufffer_Size];
uint8_t U1_TX_Buffer[U1TX_Bufffer_Size];


//数据控制结构体
UCB_ControlBlock UCB_CB;

//记录单次接收数据的长度，防止编译器优化
volatile uint8_t Rx_Len = 0;


/**
 * @brief   初始化各个指针
 * @param   None
 * @retval  None
 */
 void UCBRx_PtrInit(void)
{
	UCB_CB.URxDataIn = &UCB_CB.URxDataBuffer[0];
	UCB_CB.URxDataOut = &UCB_CB.URxDataBuffer[0];
	UCB_CB.URxDataEnd = &UCB_CB.URxDataBuffer[URXData_SE_Size-1];//指针组最后1位作为边界
	UCB_CB.URxDataIn->start = U1_RX_Buffer;	//IN的起指针需要控制缓冲区的第1个元素
	UCB_CB.U1RX_Counter = 0;					//累计接收的数据长度
}
 

/**
 * @brief   初始化指针，并开启DMA空闲中断接收
 * @param   None
 * @retval  None
 */
void U1BootLoader_Init(void)
{
	UCBRx_PtrInit();
	__HAL_UART_ENABLE_IT(&Bootloader_Handle,UART_IT_IDLE);
	HAL_UART_Receive_DMA(&Bootloader_Handle,UCB_CB.URxDataIn->start,U1RX_Data_Max);//注意DMA配置的时候使用了循环模式，需要手动调用一次才可以形成闭环
	//第一次调用后，会进入空闲中断状态，在空闲中断中再调用一次，即可闭环
}


/**
 * @brief   U1串口空闲中断回调函数 
 * @param   串口x
 * @retval  None
 */
void U1RXBootLoader_IDLE_CallBack(UART_HandleTypeDef *huart)
{
	//清除中断标志位
	if(huart->Instance == USART1 &&
		__HAL_UART_GET_FLAG(&Bootloader_Handle,UART_IT_IDLE))
	{
		
      __HAL_UART_CLEAR_IDLEFLAG(huart);
	}
	
	//计算接收数据长度，注意比如设置单次DMA最大接收256个字节，那么假如来了200，那么余量=256-200
	//防止计算过程中被其他任务打断
   __disable_irq();
    Rx_Len = U1RX_Data_Max - __HAL_DMA_GET_COUNTER(huart -> hdmarx);//单次长度=单次数据max - dma的通道空余量
    __enable_irq();
	
	UCB_CB.U1RX_Counter += Rx_Len;	//累加单次长度，记录一回合数据的总长度
	UCB_CB.URxDataIn->end = &U1_RX_Buffer[UCB_CB.U1RX_Counter-1];	//IN的止指针位置赋值
	UCB_CB.URxDataIn++;//IN指针组上一个元素组已标记完成，使用下一个元素组,注意这里是指针地址++
	
	if(UCB_CB.URxDataIn == UCB_CB.URxDataEnd)	//判断SE元素是否到最后一位，防止越界		
	{
        UCB_CB.URxDataIn = &UCB_CB.URxDataBuffer[0];//如果是，指针数组回卷
	}

	
	if(U1RX_Bufffer_Size - UCB_CB.U1RX_Counter >= 256)	//判断缓冲区数组是否满，若没满则更新start指针位置		
	{
        UCB_CB.URxDataIn->start = &U1_RX_Buffer[UCB_CB.U1RX_Counter];
	}
	else
	{											//如果满了，开启RX缓冲区回卷
		UCB_CB.URxDataIn->start = U1_RX_Buffer;
		UCB_CB.U1RX_Counter = 0;
	}

	
	
    HAL_UART_AbortReceive(&Bootloader_Handle);               // 终止当前DMA接收,防止上次DMA的任务状态与新DMA搬运的数据冲突
    HAL_UART_Receive_DMA(&Bootloader_Handle, UCB_CB.URxDataIn->start, U1RX_Data_Max); // 重启DMA
    Rx_Len = 0;

	
	

}





/**
 * @brief   U1 Printf重定向
 * @param   format 格式字符串
 * @retval  None
 */
void U1_Printf(char *format, ...)
{
    va_list listdata;
    va_start(listdata, format);
    int len = vsnprintf((char*)U1_TX_Buffer, sizeof(U1_TX_Buffer), format, listdata);
    va_end(listdata);
 
    if (len > 0) {
        // 一次性发送有效数据（非整个数组）
        HAL_UART_Transmit(&Bootloader_Handle, U1_TX_Buffer, len, 100);
    }
    // 可选：等待最后一个字节发送完成（确保硬件完成），发生完成标志位为1
    while (__HAL_UART_GET_FLAG(&Bootloader_Handle, UART_FLAG_TC) == RESET);
}




/**
* @brief   串口1DMA不定长空闲中断验证 
 * @param   None
 * @retval  None
 */
void test_IDLE_UART1(void)
{
	
	    if (UCB_CB.URxDataOut != UCB_CB.URxDataIn) {
			uint16_t len = UCB_CB.URxDataOut->end - UCB_CB.URxDataOut->start + 1;	//中间变量要用16位，因为8位=255，有可能一次来数据256Max
			U1_Printf("接收数据长度为：%d\r\n",len);
			
			for(uint8_t i = 0;i<len;i++)
            {
				U1_Printf("%c",UCB_CB.URxDataOut->start[i]);

            }
			U1_Printf("\r\n");
			UCB_CB.URxDataOut++;	//本轮数据解析完成，开始指向下一段数据
			
			if(UCB_CB.URxDataOut == UCB_CB.URxDataEnd)
			{
				UCB_CB.URxDataOut = &UCB_CB.URxDataBuffer[0];//重新回卷解析
			}
		}
}















