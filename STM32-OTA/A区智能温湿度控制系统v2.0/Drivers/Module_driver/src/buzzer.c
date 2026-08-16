
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "buzzer.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */



/* USER CODE BEGIN 2 */
/**
 * @brief       初始化BEEP相关IO口, 并使能时钟
 * @param       无
 * @retval      无
 */
void beep_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    BEEP_GPIO_CLK_ENABLE();                             /* BEEP时钟使能 */

    gpio_init_struct.Pin = BEEP_GPIO_PIN;               /* 蜂鸣器引脚 */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;        /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;      /* 高速 */
    HAL_GPIO_Init(BEEP_GPIO_PORT, &gpio_init_struct);   /* 初始化蜂鸣器引脚 */

    BEEP(0);                                            /* 关闭蜂鸣器 */
}

/* USER CODE END 2 */
