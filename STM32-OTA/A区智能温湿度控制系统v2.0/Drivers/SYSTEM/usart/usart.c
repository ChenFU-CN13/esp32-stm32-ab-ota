

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"


/* 如果使用os,则包括下面的头文件即可 */
//#if SYS_SUPPORT_OS
//#include "os.h"                               /* os 使用 */
//#endif

/******************************************************************************************/
/* 加入以下代码, 支持printf函数, 而不需要选择use MicroLIB */

#if 1
#if (__ARMCC_VERSION >= 6010050)                    /* 使用AC6编译器时 */
__asm(".global __use_no_semihosting\n\t");          /* 声明不使用半主机模式 */
__asm(".global __ARM_use_no_argv \n\t");            /* AC6下需要声明main函数为无参数格式，否则部分例程可能出现半主机模式 */

#else
/* 使用AC5编译器时, 要在这里定义__FILE 和 不使用半主机模式 */
#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;
    /* Whatever you require here. If the only file you are using is */
    /* standard output using printf() for debugging, no file handling */
    /* is required. */
};

#endif

/* 不使用半主机模式，至少需要重定义_ttywrch\_sys_exit\_sys_command_string函数,以同时兼容AC6和AC5模式 */
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/* 定义_sys_exit()以避免使用半主机模式 */
void _sys_exit(int x)
{
    x = x;
}

char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}

/* FILE 在 stdio.h里面定义. */
FILE __stdout;

/* 重定义fputc函数, printf函数最终会通过调用fputc输出字符串到串口 */
int fputc(int ch, FILE *f)
{
    while ((USART1->SR & 0X40) == 0);               /* 等待上一个字符发送完成 */

    USART1->DR = (uint8_t)ch;                       /* 将要发送的字符 ch 写入到DR寄存器 */
    return ch;
}
#endif
/***********************************************串口1初始化*******************************************/
    
#if USART_EN_RX                                     /* 如果使能了接收 */

/* 接收缓冲, 最大USART_REC_LEN个字节. */
uint8_t g_usart_rx_buf[USART_REC_LEN];

/*  接收状态
 *  bit15，      接收完成标志
 *  bit14，      接收到0x0d
 *  bit13~0，    接收到的有效字节数目
*/
uint16_t g_usart_rx_sta = 0;

uint8_t g_rx_buffer[RXBUFFERSIZE];                  /* HAL库使用的串口接收缓冲 */

UART_HandleTypeDef g_uart1_handle;                  /* UART句柄 */


/**
 * @brief       串口X初始化函数
 * @param       baudrate: 波特率, 根据自己需要设置波特率值
 * @note        注意: 必须设置正确的时钟源, 否则串口波特率就会设置异常.
 *              这里的USART的时钟源在sys_stm32_clock_init()函数中已经设置过了.
 * @retval      无
 */
void usart_init(uint32_t baudrate)
{
    g_uart1_handle.Instance = USART_UX;                         /* USART1 */
    g_uart1_handle.Init.BaudRate = baudrate;                    /* 波特率 */
    g_uart1_handle.Init.WordLength = UART_WORDLENGTH_8B;        /* 字长为8位数据格式 */
    g_uart1_handle.Init.StopBits = UART_STOPBITS_1;             /* 一个停止位 */
    g_uart1_handle.Init.Parity = UART_PARITY_NONE;              /* 无奇偶校验位 */
    g_uart1_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;        /* 无硬件流控 */
    g_uart1_handle.Init.Mode = UART_MODE_TX_RX;                 /* 收发模式 */
    HAL_UART_Init(&g_uart1_handle);                             /* HAL_UART_Init()会使能UART1 */
    
    /* 该函数会开启接收中断：标志位UART_IT_RXNE，并且设置接收缓冲以及接收缓冲接收最大数据量 */
    HAL_UART_Receive_IT(&g_uart1_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
}








/***********************************************串口2初始化*******************************************/

UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;

void uart2_send_complete_CB(UART_HandleTypeDef *huart);





void USART2_Init(uint32_t baudrate)
{
	  huart2.Instance = USART2;
	  huart2.Init.BaudRate = baudrate;
	  huart2.Init.WordLength = UART_WORDLENGTH_8B;
	  huart2.Init.StopBits = UART_STOPBITS_1;
	  huart2.Init.Parity = UART_PARITY_NONE;
	  huart2.Init.Mode = UART_MODE_TX_RX;
	  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	  if (HAL_UART_Init(&huart2) != HAL_OK)
	  {
		Error_Handler();
	  }

	  HAL_UART_RegisterCallback(&huart2,HAL_UART_TX_COMPLETE_CB_ID,uart2_send_complete_CB);//注册回调，不影响发送接收，用于做下一步处理
	
	
}









/**
 * @brief       UART底层初始化函数
 * @param       huart: UART句柄类型指针
 * @note        此函数会被HAL_UART_Init()调用
 *              完成时钟使能，引脚配置，中断配置
 * @retval      无
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio_init_struct;
    if(huart->Instance == USART_UX)                             /* 如果是串口1，进行串口1 MSP初始化 */
    {
        USART_UX_CLK_ENABLE();                                  /* USART1 时钟使能 */
        USART_TX_GPIO_CLK_ENABLE();                             /* 发送引脚时钟使能 */
        USART_RX_GPIO_CLK_ENABLE();                             /* 接收引脚时钟使能 */

        gpio_init_struct.Pin = USART_TX_GPIO_PIN;               /* TX引脚 */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                /* 复用推挽输出 */
        gpio_init_struct.Pull = GPIO_PULLUP;                    /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;          /* 高速 */
        gpio_init_struct.Alternate = USART_TX_GPIO_AF;          /* 复用为USART1 */
        HAL_GPIO_Init(USART_TX_GPIO_PORT, &gpio_init_struct);   /* 初始化发送引脚 */

        gpio_init_struct.Pin = USART_RX_GPIO_PIN;               /* RX引脚 */
        gpio_init_struct.Alternate = USART_RX_GPIO_AF;          /* 复用为USART1 */
        HAL_GPIO_Init(USART_RX_GPIO_PORT, &gpio_init_struct);   /* 初始化接收引脚 */

#if USART_EN_RX
        HAL_NVIC_EnableIRQ(USART_UX_IRQn);                      /* 使能USART1中断通道 */
        HAL_NVIC_SetPriority(USART_UX_IRQn, 3, 3);              /* 抢占优先级3，子优先级3 */
		
		 
#endif
    }
	
	
	/********串口2配置********/
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(huart->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2 DMA Init */
    /* USART2_RX Init */
    hdma_usart2_rx.Instance = DMA1_Stream5;
    hdma_usart2_rx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart2_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart2_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_rx.Init.Mode = DMA_NORMAL;
    hdma_usart2_rx.Init.Priority = DMA_PRIORITY_MEDIUM;
    hdma_usart2_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart2_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(huart,hdmarx,hdma_usart2_rx);

    /* USART2_TX Init */
    hdma_usart2_tx.Instance = DMA1_Stream6;
    hdma_usart2_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart2_tx.Init.Mode = DMA_NORMAL;
    hdma_usart2_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
    hdma_usart2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(huart,hdmatx,hdma_usart2_tx);

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
		
		

		
		

    }
}


void USART2_DMA1_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

}


/**
 * @brief       Rx传输回调函数
 * @param       huart: UART句柄类型指针
 * @retval      无
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART_UX)             /* 如果是串口1 */
    {
        if((g_usart_rx_sta & 0x8000) == 0)      /* 接收未完成 */
        {
            if(g_usart_rx_sta & 0x4000)         /* 接收到了0x0d */
            {
                if(g_rx_buffer[0] != 0x0a) 
                {
                    g_usart_rx_sta = 0;         /* 接收错误,重新开始 */
                }
                else 
                {
                    g_usart_rx_sta |= 0x8000;   /* 接收完成了 */
                }
            }
            else                                /* 还没收到0X0D */
            {
                if(g_rx_buffer[0] == 0x0d)
                {
                    g_usart_rx_sta |= 0x4000;
                }
                else
                {
                    g_usart_rx_buf[g_usart_rx_sta & 0X3FFF] = g_rx_buffer[0] ;
                    g_usart_rx_sta++;
                    if(g_usart_rx_sta > (USART_REC_LEN - 1))
                    {
                        g_usart_rx_sta = 0;     /* 接收数据错误,重新开始接收 */
                    }
                }
            }
        }
        
        HAL_UART_Receive_IT(&g_uart1_handle, (uint8_t *)g_rx_buffer, RXBUFFERSIZE);
    }

}


void uart2_send_complete_CB(UART_HandleTypeDef *huart)
{

	if(huart->Instance == USART2)
	{
        g_uart2_tx_state = UART_TX_IDLE;// 发送完成，更新状态为空闲

	}
}




/**
 * @brief       串口1中断服务函数
 * @param       无
 * @retval      无
 */
void USART_UX_IRQHandler(void)
{ 
    HAL_UART_IRQHandler(&g_uart1_handle);       /* 调用HAL库中断处理公用函数 */
	
}

#endif


 

 /* 必须在usart.c中补充这两个中断服务函数
如果没有流的中断响应，会*/
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_rx); // 转发给HAL库处理RX DMA中断
}

void DMA1_Stream6_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart2_tx); // 转发给HAL库处理TX DMA中断
}

/* 补充USART2中断服务函数（如果还没实现） */
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}




