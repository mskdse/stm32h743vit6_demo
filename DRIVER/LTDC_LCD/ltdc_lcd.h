#ifndef _LTDC_LCD_H_
#define _LTDC_LCD_H_

#include "stm32h7xx_hal.h"
#include <stdbool.h>

/* 手册中的水平同步和帧同步长度 */
#define HSYNC_LEN     48
#define VSYNC_LEN      3

/* 手册中的水平后廊和垂直后廊长度 */
#define HBP_LEN       88
#define VBP_LEN       32

/* 手册中的水平前廊和垂直前廊长度 */
#define HFP_LEN       40
#define VFP_LEN       13

/* LCD的长宽 */
#define LCD_WIDTH     800
#define LCD_HEIGH     480

/* 受限于内存限制，所以最大只能显示这么点 */
#define LCD_WIN_X0      0
#define LCD_WIN_X1    640
#define LCD_WIN_Y0      0
#define LCD_WIN_Y1    400

void ltdc_lcd_init(void);
void ltdc_lcd_bl_set(uint32_t freq,uint8_t paluse);

#endif
