/**
 * @file    command.c
 * @brief   串口环形缓冲区实现 - 双缓冲区方案
 * @details DMA临时区接收后转存软件环；软件环采用累计计数，整批写入或整批拒收。
 *          DMA空闲事件只表示接收片段结束，不保证得到一帧完整的OTA报文。
 */

#include "command.h"


/*============================================================================*/
/*                          私有变量定义                                       */
/*============================================================================*/

/*DMA二级接收缓冲区·环形缓冲区*/
static uint8_t  uart_rx_buffer[UART_RX_BUFFER_SIZE];

/* 累计计数器，操作数组读写采用掩码操作。 */
static volatile uint32_t uart_wr = 0U;
static volatile uint32_t uart_rd = 0U;
 
/* 软件环空间不足时，累计本批未接收的字节数。 */
static volatile uint32_t uart_overflow_bytes = 0U;  //只在usart2空闲中断中被修改

/*DMA一级接收缓冲区·临时缓冲区*/
static uint8_t dma_temp_buffer[UART_DMA_RX_MAX];
static volatile uint16_t dma_rx_len = 0U;

/* 空闲中断回调会先调用本函数，因此在定义之前声明。 */
static uint16_t UART_RingWriteFromISR(const uint8_t *src, uint16_t len);



/*============================================================================*/
/*                          接口函数实现                                       */
/*============================================================================*/

/**
 * @brief 清空软件环，并启动USART2的DMA接收。
 * @param 无。
 * @return 无。
 * @note 只在启动阶段调用，此时不能有其他代码正在读写软件环。
 */
void UART_circular_buffer_Init(void)
{
    /* 必须分别赋值；逗号表达式不会同时清零两个计数器。 */
    uart_rd = 0U;
    uart_wr = 0U;
    uart_overflow_bytes = 0U;
    dma_rx_len = 0U;

    __HAL_UART_CLEAR_IDLEFLAG(UART_HANDLE);//清除串口空闲中断标志

    if (HAL_UART_Receive_DMA(UART_HANDLE, dma_temp_buffer,
                             UART_DMA_RX_MAX) != HAL_OK)
    {
        Error_Handler();
    }
    
    __HAL_UART_ENABLE_IT(UART_HANDLE,UART_IT_IDLE);

}




/**
 * @brief   串口空闲中断回调函数 
 * @param   串口句柄 
 * @retval  None
 * @note    UART_IDLE_CallBack要在HAL_UART_IRQHandler中调用
 * 
 * @details 双缓冲区工作原理:
 *          1. DMA接收到临时缓冲区dma_temp_buffer
 *          2. 空闲中断触发时，将数据复制到环形缓冲区
 *          3. 复制完成后，重新启动DMA接收
 */
void UART_IDLE_CallBack(UART_HandleTypeDef *huart)
{

    
    if(huart == &huart2 && __HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE))  
    {
        __HAL_UART_CLEAR_IDLEFLAG(UART_HANDLE);//清除串口空闲中断标志
        
        dma_rx_len = UART_DMA_RX_MAX - __HAL_DMA_GET_COUNTER(huart->hdmarx);//获取DMA接收数据长度，
        //原理：DMA接收数据长度=DMA接收缓冲区大小-当前DMA接收指针位置
        //例如：DMA接收缓冲区大小=1024，当前DMA接收指针位置=1023，那么DMA接收数据长度=1024-1023=1
        if(dma_rx_len > 0 && dma_rx_len <= UART_DMA_RX_MAX)//如果DMA接收数据长度大于0，且小于等于DMA接收缓冲区大小
        {
            /*
            * 空间足够时整批转存；空间不足时函数内部统计拒收字节。
            */
            (void)UART_RingWriteFromISR(dma_temp_buffer, dma_rx_len);
        }
        
        HAL_UART_AbortReceive(huart);//取消当前DMA接收
        HAL_UART_Receive_DMA(huart, dma_temp_buffer, UART_DMA_RX_MAX);//重新启动DMA接收
    }
}




/**
 * @brief 获取主循环本次看到的未读字节数。
 * @param 无。
 * @return 未读字节数，正常范围为0到UART_RX_BUFFER_SIZE。
 * @note 只在唯一读取者所在的主循环调用。
 */
uint16_t UART_circular_buffer_GetRxLen(void)
{
    uint32_t WR,RD;

    RD = uart_rd;
    WR = uart_wr;

    return (uint16_t)(WR - RD);
}



/**
 * @brief 在接收中断中，把一批数据转存到软件环。
 * @param src 本批数据的起始地址，必须指向len字节有效内存。
 * @param len 本批数据长度。
 * @return len：整批写入成功；0：参数无效或空间不足。
 * @note 只能由唯一接收中断调用，src不能与软件环数组重叠。
 *       空间不足时整批拒收，不覆盖旧数据，也不只写入半批。
 */
static uint16_t UART_RingWriteFromISR(const uint8_t *src,
                                      uint16_t len)
{
    uint32_t wr;
    uint32_t rd;
    uint32_t used;
    uint16_t i;

    if ((src == NULL) || (len == 0U))
    {
        return 0U;
    }

    wr = uart_wr;
    rd = uart_rd;
    used = (uint32_t)(wr - rd);

    /*
     * 先检查used，避免容量减去异常的大数后发生无符号回绕。
     * 正常运行时used不会超过容量。
     */
    if (used > UART_RX_BUFFER_SIZE)
    {
        uart_overflow_bytes += len;
        return 0U;
    }

    /* 剩余空间必须能容纳本批全部数据，否则丢弃，等待下一帧重新发。 */
    if ((uint32_t)len > (UART_RX_BUFFER_SIZE - used))
    {
        uart_overflow_bytes += len;
        return 0U;
    }

    __DMB();

    for (i = 0U; i < len; i++)
    {
        uart_rx_buffer[(wr + i) & UART_RX_BUFFER_MASK] = src[i];
    }
    __DMB();
    uart_wr = wr + len;

    return len;
}


/**
 * @brief 从软件环取出最早到达的一批数据。
 * @param buf 保存读出数据的数组，不能与环形缓冲区数组重叠。
 * @param len 希望读取的字节数，buf至少提供这些空间。
 * @return 实际读取字节数；参数无效或没有数据时返回0。
 * @note 只能由Bootloader主循环读取，全部拷完之后才释放位置。
 */
uint16_t UART_circular_buffer_ReadData(uint8_t *buf, uint16_t len)
{
    uint32_t WR,RD,USED;
    uint16_t i;

    if ((buf == NULL) || (len == 0U))
    {
        return 0U;
    }

    RD = uart_rd;
    WR = uart_wr;

    USED = (uint32_t)(WR - RD);
    if (USED == 0U) return 0U;

    if ((uint32_t)len > USED) len = (uint16_t)USED;

    __DMB();
    for (i = 0U; i < len; i++)
    {
        buf[i] = uart_rx_buffer[(RD + i) & UART_RX_BUFFER_MASK];
    }
    __DMB();
    uart_rd = RD + len; /* 拷出后发布新的读计数，释放这些位置。 */

    return len;

}


/**
 * @brief 查看软件环开头的数据，不取走。
 * @param buf 保存查看结果的数组。
 * @param len 最多查看多少字节，buf至少提供这些空间。
 * @return 实际复制的字节数。
 * @note 只能由Bootloader主循环调用，调用后读计数不变。
 */
uint16_t UART_circular_buffer_PeekData(uint8_t *buf, uint16_t len)
{
    uint32_t rd;
    uint32_t wr;
    uint32_t used;
    uint16_t i;

    if ((buf == NULL) || (len == 0U))
    {
        return 0U;
    }

    rd = uart_rd;
    wr = uart_wr;
    used = (uint32_t)(wr - rd);

    if ((uint32_t)len > used)
    {
        len = (uint16_t)used;
    }

    __DMB();

    for (i = 0U; i < len; i++)
    {
        buf[i] = uart_rx_buffer[(rd + i) & UART_RX_BUFFER_MASK];
    }
    /* 不推进uart_rd，查看过的数据仍然留在软件环里。 */
    return len;
}

/**
 * @brief 丢弃软件环里当前已经收到的数据。
 * @param 无。
 * @return 无。
 * @note 只能由Bootloader主循环调用，不停止DMA，不清零写计数。
 */
void UART_circular_buffer_Clear(void)
{
    uint32_t wr = uart_wr;

    __DMB();
    /* 只推进读计数；取快照之后新写入的数据仍然保留。 */
    uart_rd = wr;
}

/**
 * @brief 获取软件环因空间不足而拒收的字节数。
 * @param 无。
 * @return 软件环拒收字节累计值，允许自然回绕。
 * @note 只用于诊断，不代表UART和DMA链路的全部丢字节数量。
 */
uint32_t UART_circular_buffer_GetOverflowCount(void)
{
    return uart_overflow_bytes;
}


/**
 * @brief 在主循环中执行一次独立的串口回显测试。
 * @param 无。
 * @return 无。
 * @note 不得在中断中调用，也不能与OTA协议同时运行。
 *       使用带超时的阻塞发送，保证函数返回后不再访问局部数组。
 */
void test(void)
{
    uint8_t buffer[256];
    uint16_t len;

    len = UART_circular_buffer_ReadData(buffer, (uint16_t)sizeof(buffer));
    if (len == 0U)
    {
        return;
    }

    if (HAL_UART_Transmit(UART_HANDLE, buffer, len, 100U) == HAL_OK)
    {
        U1_Printf("TEST OK!");
    }
    else
    {
        U1_Printf("[UART2] 回显发送失败\r\n");
    }
}
