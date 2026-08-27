#ifndef __DELAY_H
#define __DELAY_H

#include "./SYSTEM/sys/sys.h"

void delay_init(void);          /* 延时初始化 */
void delay_us(uint32_t nus);    
void delay_ms(uint32_t nms);   
void HAL_Delay(uint32_t Delay); 

#endif
