#ifndef _LTDC_LCD_DISP_JPEG_H_
#define _LTDC_LCD_DISP_JPEG_H_

#include "ltdc_lcd.h"
#include <string.h>  
#include <string.h>
#include <stdint.h>
#include <stdio.h>

void ltdc_lcd_disp_jpeg_init(void);
HAL_StatusTypeDef ltdc_lcd_disp_jpeg_show(const char *filename);

#endif
