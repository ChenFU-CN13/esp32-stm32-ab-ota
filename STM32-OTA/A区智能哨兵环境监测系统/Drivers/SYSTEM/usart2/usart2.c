
#include "./SYSTEM/usart2/usart2.h"


/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

//UART_HandleTypeDef huart2;
//DMA_HandleTypeDef hdma_usart2_rx;
//DMA_HandleTypeDef hdma_usart2_tx;

//void uart2_send_complete_CB(UART_HandleTypeDef *huart);


//const uint8_t cmd[]="free_malloc_runing!";
//uint8_t* buff = NULL; 
/* USART2 init function */



//void HAL_USART2_Init(UART_HandleTypeDef* uartHandle)
//{
//	huart2.Instance = USART2;
//	huart2.Init.BaudRate = 115200;
//	huart2.Init.WordLength = UART_WORDLENGTH_8B;
//	huart2.Init.StopBits = UART_STOPBITS_1;
//	huart2.Init.Parity = UART_PARITY_NONE;
//	huart2.Init.Mode = UART_MODE_TX_RX;
//	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
//	
//	if(HAL_UART_Init(&huart2)!=HAL_OK)
//	{
//		Error_Handler();
//	}
//	
//	HAL_UART_RegisterCallback(&huart2,HAL_UART_TX_COMPLETE_CB_ID,uart2_send_complete_CB);//注册回调
//}


//// HAL_UART_MspInit：串口硬件底层初始化（GPIO/DMA/时钟）
//// 这个函数会被HAL_UART_Init自动调用，不要手动调用！
//void HAL_USART2_MspInit(UART_HandleTypeDef* uartHandle)
//{
//	GPIO_InitTypeDef GPIO_InitStruct = {0};

//	if(uartHandle->Instance == USART2)
//    {
//		// 1. 使能时钟（仅调用一次）
//		__HAL_RCC_USART2_CLK_ENABLE();
//		__HAL_RCC_GPIOA_CLK_ENABLE();
//		__HAL_RCC_DMA1_CLK_ENABLE();
//		
//		//2.gpio初始化
//		GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;//PA2->TX,PA3->RX
//		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//		GPIO_InitStruct.Pull = GPIO_NOPULL;
//		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//		GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
//		HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);
//		
//		//3.DMA初始化
//		hdma_usart2_rx.Instance = DMA1_Stream5;
//		hdma_usart2_rx.Init.Channel = DMA_CHANNEL_4;
//		hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
//		hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
//		hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
//		hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//		hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//		hdma_usart2_rx.Init.Mode = DMA_NORMAL;
//		hdma_usart2_rx.Init.Priority = DMA_PRIORITY_LOW;
//		hdma_usart2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
//		
//		if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK)
//		{
//		  Error_Handler();
//		}
//		__HAL_LINKDMA(uartHandle,hdmatx,hdma_usart2_tx);
//		
//		
//		
//		
//		hdma_usart2_tx.Instance = DMA1_Stream6;
//		hdma_usart2_tx.Init.Channel = DMA_CHANNEL_4;
//		hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
//		hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
//		hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
//		hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//		hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//		hdma_usart2_tx.Init.Mode = DMA_NORMAL;
//		hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW;
//		hdma_usart2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
//		if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK)
//		{
//		  Error_Handler();
//		}
//		__HAL_LINKDMA(uartHandle, hdmatx, hdma_usart2_tx);
//		
//		// 4. 中断配置（适配FreeRTOS）
//		// 抢占优先级≥configMAX_SYSCALL_INTERRUPT_PRIORITY（假设configMAX=5）
//		HAL_NVIC_SetPriority(USART2_IRQn, 6, 3);
//		HAL_NVIC_EnableIRQ(USART2_IRQn);
//		
//		// DMA1_Stream5（RX）中断
//		  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 6, 0);
//		  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
//		  
//		  // DMA1_Stream6（TX）中断
//		  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 6, 1);
//		  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
//		
//    }
//}



//void uart2_send_complete_CB(UART_HandleTypeDef *huart)
//{

//	if(huart->Instance == USART2 && buff != NULL)
//	{
//		free(buff);
//		buff = NULL;

//		printf("555");
//	}
//}

//void test_mymalloc(void)
//{
//	buff = malloc(strlen((char*)cmd));
//	if(buff==NULL)
//    {
//		uint8_t print1[]="free_malloc_failed";
//		HAL_UART_Transmit_DMA(&huart2,(uint8_t*)print1,strlen((char*)print1));
//    }
//	else
//	{
//		uint8_t print2[]="free_malloc_runing";
//		memcpy(buff,print2,strlen((char*)print2));
//		HAL_UART_Transmit_DMA(&huart2,(uint8_t*)buff,strlen((char*)buff));
//	}
//}
/* USER CODE END 1 */



 

 




