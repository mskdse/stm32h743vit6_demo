#ifndef _LTDC_LCD_H_
#define _LTDC_LCD_H_

#include "stm32h7xx_hal.h"
#include <stdbool.h>

void ltdc_lcd_init(void);
void ltdc_lcd_bl_set(uint32_t freq,uint8_t paluse);

#endif
