/**
 * @file    command.c
 * @brief   串口环形缓冲区实现 - 双缓冲区方案
 * @details 使用DMA空闲中断接收，双缓冲区避免数据竞争
 */

#include "command.h"


/*============================================================================*/
/*                          私有变量定义                                       */
/*============================================================================*/

/*DMA二级接收缓冲区·环形缓冲区*/
static uint8_t  uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t uart_w_index = 0;
static volatile uint16_t uart_r_index = 0;
 
/*DMA一级接收缓冲区·临时缓冲区*/
static uint8_t  dma_temp_buffer[UART_DMA_RX_MAX];
static volatile uint16_t dma_rx_len = 0;
static volatile uint8_t  dma_rx_ready = 0;




/*============================================================================*/
/*                          接口函数实现                                       */
/*============================================================================*/

/**
 * @brief  初始化：接收DMA + 空闲中断
 */
void UART_circular_buffer_Init(void)
{
    uart_w_index = 0;
    uart_r_index = 0;
    dma_rx_len = 0;
    dma_rx_ready = 0;
    
    __HAL_UART_ENABLE_IT(UART_HANDLE, UART_IT_IDLE);
    
    HAL_UART_Receive_DMA(UART_HANDLE, dma_temp_buffer, UART_DMA_RX_MAX);
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
    uint16_t i;
    uint16_t copy_len;
    
    if(huart == &huart2 && __HAL_UART_GET_FLAG(huart, UART_FLAG_IDLE))  
    {
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_IDLE);//清除空闲标志位
        
        dma_rx_len = UART_DMA_RX_MAX - __HAL_DMA_GET_COUNTER(huart->hdmarx);//获取DMA接收数据长度，
        //原理：DMA接收数据长度=DMA接收缓冲区大小-当前DMA接收指针位置
        //例如：DMA接收缓冲区大小=1024，当前DMA接收指针位置=1023，那么DMA接收数据长度=1024-1023=1
        if(dma_rx_len > 0 && dma_rx_len <= UART_DMA_RX_MAX)//如果DMA接收数据长度大于0，且小于等于DMA接收缓冲区大小
        //确保数据不丢失
        {
            for(i = 0; i < dma_rx_len; i++) //将DMA接收数据复制到环形缓冲区
            //确保数据不丢失
            {
                uart_rx_buffer[uart_w_index] = dma_temp_buffer[i]; //将DMA接收数据复制到环形缓冲区
                uart_w_index = (uart_w_index + 1) % UART_RX_BUFFER_SIZE; //更新写入索引
                //确保写入索引不超出环形缓冲区大小
                //例如：如果写入索引=1023，那么写入索引=0
            }
        }
        
        HAL_UART_AbortReceive(huart);//取消当前DMA接收
        HAL_UART_Receive_DMA(huart, dma_temp_buffer, UART_DMA_RX_MAX);//重新启动DMA接收
    }
}




/**
 * @brief  获取当前未读数据长度
 * @retval 未读长度：0~UART_RX_BUFFER_SIZE
 */
uint16_t UART_circular_buffer_GetRxLen(void)
{
    if (uart_w_index >= uart_r_index) //如果写入索引大于等于读取索引
    {
        return uart_w_index - uart_r_index; //返回未读数据长度
    }
    else
    {
        return (UART_RX_BUFFER_SIZE - uart_r_index) + uart_w_index; //返回未读数据长度
    }
}




/**
 * @brief  从环形缓冲区读取数据
 * @param  buf  数据缓冲区
 * @param  len  数据长度
 * @retval 读取到的数据长度
 */
uint16_t UART_circular_buffer_ReadData(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t total_len = UART_circular_buffer_GetRxLen(); //获取当前未读数据长度

    if(len > total_len) len = total_len; //如果读取数据长度大于未读数据长度，

    if(len == 0) return 0; //如果读取数据长度为0，返回0


    for (i = 0; i < len; i++) //将环形缓冲区数据复制到数据缓冲区
    //例如：如果环形缓冲区数据长度=1024，读取数据长度=1023，那么读取数据长度=1023
    {
        buf[i] = uart_rx_buffer[uart_r_index];
        uart_r_index = (uart_r_index + 1) % UART_RX_BUFFER_SIZE;
    }
    
    return len;
}




/**
 * @brief  测试函数
 */
void test(void)
{
    if (UART_circular_buffer_GetRxLen() > 0) //如果当前未读数据长度大于0
    {
        uint8_t buffer[256];
        uint16_t len = UART_circular_buffer_ReadData(buffer, sizeof(buffer)); //读取当前未读数据
        HAL_UART_Transmit_DMA(UART_HANDLE, buffer, len); //发送数据
        U1_Printf("TEST OK!");
    }
}
