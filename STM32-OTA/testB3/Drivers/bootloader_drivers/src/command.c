/**
 * @file    command.c
 * @brief   串口环形缓冲区实现 - 双缓冲区方案
 * @details 使用DMA空闲中断接收，双缓冲区避免数据竞争
 */

#include "command.h"


/*============================================================================*/
/*                          私有变量定义                                       */
/*============================================================================*/

static uint8_t  uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t uart_w_index = 0;
static volatile uint16_t uart_r_index = 0;

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
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_IDLE);
        
        dma_rx_len = UART_DMA_RX_MAX - __HAL_DMA_GET_COUNTER(huart->hdmarx);
        
        if(dma_rx_len > 0 && dma_rx_len <= UART_DMA_RX_MAX)
        {
            for(i = 0; i < dma_rx_len; i++)
            {
                uart_rx_buffer[uart_w_index] = dma_temp_buffer[i];
                uart_w_index = (uart_w_index + 1) % UART_RX_BUFFER_SIZE;
            }
        }
        
        HAL_UART_AbortReceive(huart);
        HAL_UART_Receive_DMA(huart, dma_temp_buffer, UART_DMA_RX_MAX);
    }
}




/**
 * @brief  获取当前未读数据长度
 * @retval 未读长度：0~UART_RX_BUFFER_SIZE
 */
uint16_t UART_circular_buffer_GetRxLen(void)
{
    if (uart_w_index >= uart_r_index)
    {
        return uart_w_index - uart_r_index;
    }
    else
    {
        return (UART_RX_BUFFER_SIZE - uart_r_index) + uart_w_index;
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
    uint16_t total_len = UART_circular_buffer_GetRxLen();

    if(len > total_len) len = total_len;
    if(len == 0) return 0;

    for (i = 0; i < len; i++)
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
    if (UART_circular_buffer_GetRxLen() > 0)
    {
        uint8_t buffer[256];
        uint16_t len = UART_circular_buffer_ReadData(buffer, sizeof(buffer));
        HAL_UART_Transmit_DMA(UART_HANDLE, buffer, len);
        U1_Printf("TEST OK!");
    }
}
