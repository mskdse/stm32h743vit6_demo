#ifndef _LTDC_LCD_H_
#define _LTDC_LCD_H_

#include "stm32h7xx_hal.h"
#include <stdbool.h>

/* 手册中的水平同步和帧同步长度 */
#define HSYNC_LEN     48
#define VSYNC_LEN      3

/* 手册中的水平后廊和垂直后廊长度 */
#define HBP_LEN       40
#define VBP_LEN       29

/* 手册中的水平前廊和垂直前廊长度 */
#define HFP_LEN       40
#define VFP_LEN       13

/* LCD的长宽 */
#define LCD_WIDTH     800
#define LCD_HEIGH     480

/* 受限于内存限制，所以最大只能显示这么点 */
#define LCD_WIN_X0      0
#define LCD_WIN_X1    540
#define LCD_WIN_Y0      0
#define LCD_WIN_Y1    430

void ltdc_lcd_init(void);
void ltdc_lcd_bl_set(uint32_t freq,uint8_t paluse);

void ltdc_lcd_dma2d_fill(uint16_t x,uint16_t xsize,uint16_t y,uint16_t ysize,uint16_t color);
void ltdc_lcd_dma2d_data_copy(const uint16_t* data_src,uint16_t x,uint16_t xsize,uint16_t y,uint16_t ysize);
void ltdc_lcd_dma2d_data_fusion(const uint16_t* data_src,uint16_t x,uint16_t xsize,uint16_t y,uint16_t ysize,uint8_t fusion);

#endif
