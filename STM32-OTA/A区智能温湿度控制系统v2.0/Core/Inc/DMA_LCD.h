#ifndef __DMA_LCD_H__
#define __DMA_LCD_H__

#ifdef __cplusplus
extern "C" {
#endif


#include "main.h"
void dma_lcd_start_transfer(uint16_t *data, uint32_t size);
uint8_t dma_lcd_is_transfer_complete(void);
void dma_lcd_init(void);
#ifdef __cplusplus
}
#endif
#endif 
