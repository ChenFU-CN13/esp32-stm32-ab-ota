#include "TaskTest.h"


uint8_t uart2_buffer[3] = {0};
uint8_t uart2_ACK[3] = {0X66,0X02,0XBB};

void testtask(void)
{
	
	
	
	
	HAL_UART_Receive_DMA(&UART_port,uart2_buffer,sizeof(uart2_buffer));
	if(uart2_buffer[0] == 0X66 && uart2_buffer[2] == 0XBB)
	{
		uint8_t TEMP = uart2_buffer[1];
		switch(TEMP)
        {
           case 0X01:
			   
			   HAL_UART_Transmit(&UART_port,uart2_ACK,sizeof(uart2_ACK),100);
			   memset(uart2_buffer,0,sizeof(uart2_buffer));
				U1_Printf("第一次握手成功 \r\n");
				HAL_NVIC_SystemReset();
             break;
        }
	
	}




}














