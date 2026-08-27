#include "./SYSTEM/delay/delay.h"
#include "./BSP/TIMER/btim.h"

/**
 * @brief  基于定时器6的延时
 * @note    配置TIM6为1MHZ的计数频率（1MHZ=1US）
 */
void delay_init(void)
{
    /* 
     * 计数频率 = 84MHz / (psc + 1) = 1MHz -> psc = 83
     * 自动重装值arr设置为0xFFFF(16位最大值),确保单次计数可覆盖65535us(~65ms)
     */
    btim_timx_int_init(0xFFFF, 83);  // arr=65535,psc=83-》 1MHz
}

/**
 * @brief   
 * @param   nus：最大延时65535us
 */
void delay_us(uint32_t nus)
{
    uint32_t start, current, elapsed;
    
    // 确保不超最大范围
    if (nus > 0xFFFF) nus = 0xFFFF;
    
    // 记录起始计数
    start = BTIM_TIMX_INT->CNT;
    elapsed = 0;
    
    while (elapsed < nus)
    {
        current = BTIM_TIMX_INT->CNT;
        // 处理计数溢出（从0xffff回到0）
        if (current >= start)
        {
            elapsed = current - start;
        }
        else
        {
            elapsed = (0xFFFF - start) + current + 1;  // +1 补偿溢出边界
        }
    }
}

/**
 * @brief   
 * @param   
 */
void delay_ms(uint32_t nms)
{
    uint32_t i;
    for (i = 0; i < nms; i++)
    {
        delay_us(1000);  // ????1ms
    }
}

/**
 * @brief   用于兼容HAL库延时，重定义
 */
void HAL_Delay(uint32_t Delay)
{
    delay_ms(Delay);
}








