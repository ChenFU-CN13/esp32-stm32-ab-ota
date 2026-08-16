/**
 ****************************************************************************************************
 * @file        btim.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2021-10-15
 * @brief       基本定时器 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211015
 * 第一次发布
 *
 ****************************************************************************************************
 */

#include "./BSP/TIMER/btim.h"


TIM_HandleTypeDef g_timx_handle;         /* 定时器参数句柄 */

/**
 * @brief       基本定时器TIMX定时中断初始化函数（无中断）
 * @note
 *              基本定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              基本定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft=定时器工作频率,单位:Mhz
 *
 * @param       arr : 自动重装值。
 * @param       psc : 时钟预分频数
 * @retval      无
 */
void btim_timx_int_init(uint16_t arr, uint16_t psc)
{
    g_timx_handle.Instance = BTIM_TIMX_INT;
    g_timx_handle.Init.Prescaler = psc;
    g_timx_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_timx_handle.Init.Period = arr;
    g_timx_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;  // ??????
    HAL_TIM_Base_Init(&g_timx_handle);

#if BTIM_TIMX_DELAY_MODE
    
    HAL_TIM_Base_Start(&g_timx_handle);
#else
    
    HAL_TIM_Base_Start_IT(&g_timx_handle);
#endif
}



/**
 * @brief   定时器底层初始化
 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == BTIM_TIMX_INT)
    {
        BTIM_TIMX_INT_CLK_ENABLE();  /* 使能 */

#if !BTIM_TIMX_DELAY_MODE
        /* 只在中断模式下配置，计数模式不配置 */
        HAL_NVIC_SetPriority(BTIM_TIMX_INT_IRQn, 1, 3);
        HAL_NVIC_EnableIRQ(BTIM_TIMX_INT_IRQn);
#endif
    }
}


