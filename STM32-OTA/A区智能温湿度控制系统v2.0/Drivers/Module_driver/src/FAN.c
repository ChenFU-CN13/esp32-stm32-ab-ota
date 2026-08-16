#include "FAN.h"









/**
 * @brief       初始化风扇IO口
 * @param       无
 * @retval      无
 *              
 */
void FAN_init(void)
{
	GPIO_InitTypeDef gpio_init_struct;
	
	FAN_GPIO_CLK_ENABLE();
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_struct.Pin = FAN_GPIO_PIN_1|FAN_GPIO_PIN_2;
	gpio_init_struct.Pull = GPIO_PULLUP;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(FAN_PORT,&gpio_init_struct);
	
	FAN_OFF();//默认关闭风扇
}





