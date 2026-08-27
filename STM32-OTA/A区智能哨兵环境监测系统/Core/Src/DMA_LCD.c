#include "DMA_LCD.h"
#include "lv_port_disp.h"
#include "./BSP/LCD/lcd.h"


DMA_HandleTypeDef hdma_lcd;
static volatile uint8_t dma_transfer_done = 0;  
uint8_t dma_transfer_complete = 0;  // DMA传输完成标志位
extern lv_disp_drv_t disp_drv;  


static void dma_lcd_transfer_complete_callback(DMA_HandleTypeDef *hdma);
/**
 * @brief  初始化LCD DMA传输(基于FSMC接口)
 * @note   配置DMA用于LCD的FSMC接口数据传输
 */
void dma_lcd_init(void)
{
    __HAL_RCC_DMA2_CLK_ENABLE();  // 使能DMA2时钟(根据实际DMA通道调整)

	
	
    // 配置DMA参数
    hdma_lcd.Instance = DMA2_Stream6;  // 指定DMA通道(根据硬件手册确认)
    hdma_lcd.Init.Channel = DMA_CHANNEL_0;  
    hdma_lcd.Init.Direction = DMA_MEMORY_TO_MEMORY;  // 传输方向：内存到内存
    hdma_lcd.Init.PeriphInc = DMA_PINC_ENABLE;  // 外设地址增量使能
    hdma_lcd.Init.MemInc = DMA_MINC_DISABLE;      // 内存地址增量禁用
    hdma_lcd.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD; // 外设数据宽度：16位(LCD接口要求)
    hdma_lcd.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;   // 内存数据宽度：16位
    hdma_lcd.Init.Mode = DMA_NORMAL;  // 正常模式(非循环)
    hdma_lcd.Init.Priority = DMA_PRIORITY_HIGH;  // 中等优先级
    hdma_lcd.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
    hdma_lcd.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma_lcd.Init.MemBurst = DMA_MBURST_INC8;   // 内存突发传输：每次8个数据，16位数据宽度，对应16字节
    hdma_lcd.Init.PeriphBurst = DMA_MBURST_INC8;// 外设突发传输：每次8个数据，16位数据宽度，对应16字节
	

	
	
    if (HAL_DMA_Init(&hdma_lcd) != HAL_OK)
    {
        Error_Handler();  // 初始化失败时调用错误处理函数
    }

    // 配置DMA中断
    HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

	HAL_DMA_RegisterCallback(&hdma_lcd,HAL_DMA_XFER_CPLT_CB_ID,dma_lcd_transfer_complete_callback);
}

/**
 * @brief  启动DMA传输到LCD
 * @param  data: 待传输数据指针(16位数据)
 * @param  size: 传输数据量(单位：个，每个16位)
 */
void dma_lcd_start_transfer(uint16_t *data, uint32_t size)
{

//    dma_transfer_complete = 0;
//    // 启动DMA中断模式传输，目标地址为LCD的RAM寄存器(通过FSMC接口映射)
//    if (HAL_DMA_Start_IT(&hdma_lcd, (uint32_t)data, (uint32_t)&(LCD->LCD_RAM), size) != HAL_OK)
//    {
//        Error_Handler();
//    }
	
	
    dma_transfer_complete = 0;

    uint32_t max_single = 65535;  // STM32 DMA 单次最大传输限制
    uint32_t sent = 0;

    while (sent < size)
    {
        uint32_t curr_size = size - sent;
        if (curr_size > max_single)
            curr_size = max_single;

        // 等待DMA空闲
        while (HAL_DMA_GetState(&hdma_lcd) != HAL_DMA_STATE_READY);

        // 启动一段传输
        if (HAL_DMA_Start_IT(&hdma_lcd,
                             (uint32_t)(data + sent),
                             (uint32_t)&(LCD->LCD_RAM),
                             curr_size) != HAL_OK)
        {
            Error_Handler();
        }

        // 等待这一段传完
        while (!dma_transfer_complete);
        dma_transfer_complete = 0;

        sent += curr_size;
    }

    // 整段完成
    dma_transfer_complete = 1;
    lv_disp_flush_ready(&disp_drv);
}


static void dma_lcd_transfer_complete_callback(DMA_HandleTypeDef *hdma)
{
	
	dma_transfer_complete=1;
	
	if(hdma == &hdma_lcd)
	{
        lv_disp_flush_ready(&disp_drv);
	}

}


/**
 * @brief  检查DMA传输是否完成
 * @retval 1: 传输完成, 0: 传输未完成
 */
uint8_t dma_lcd_is_transfer_complete(void)
{
    return dma_transfer_complete;
}


/**
 * @brief  DMA2 Stream6中断服务函数
 */
void DMA2_Stream6_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&hdma_lcd);
}


