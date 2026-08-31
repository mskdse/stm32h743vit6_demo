#ifndef _DRVP_FMC_LCD_H_
#define _DRVP_FMC_LCD_H_

#include "drv_fmc_lcd.h"

/* ================= RGB565 常用颜色 ================= */
#define RGB565_BLACK        0x0000  /* 黑色 */
#define RGB565_WHITE        0xFFFF  /* 白色 */
#define RGB565_RED          0xF800  /* 红色 */
#define RGB565_GREEN        0x07E0  /* 绿色 */
#define RGB565_BLUE         0x001F  /* 蓝色 */
#define RGB565_YELLOW       0xFFE0  /* 黄色 */
#define RGB565_CYAN         0x07FF  /* 青色 */
#define RGB565_MAGENTA      0xF81F  /* 品红 */
#define RGB565_ORANGE       0xFD20  /* 橙色 */
#define RGB565_PURPLE       0x780F  /* 紫色 */
#define RGB565_PINK         0xF81F  /* 粉色 */
#define RGB565_BROWN        0xA145  /* 棕色 */
#define RGB565_GRAY         0x8410  /* 灰色 */
#define RGB565_DARK_GRAY    0x4208  /* 深灰色 */
#define RGB565_LIGHT_GRAY   0xC618  /* 浅灰色 */
#define RGB565_DARK_RED     0x8000  /* 深红 */
#define RGB565_DARK_GREEN   0x0400  /* 深绿 */
#define RGB565_DARK_BLUE    0x0010  /* 深蓝 */
#define RGB565_LIGHT_RED    0xFCA8  /* 浅红 */
#define RGB565_LIGHT_GREEN  0x87F0  /* 浅绿 */
#define RGB565_LIGHT_BLUE   0x841F  /* 浅蓝 */
#define RGB565_GOLD         0xFEA0  /* 金色 */
#define RGB565_SILVER       0xC618  /* 银色 */
#define RGB565(r, g, b) \
    (uint16_t)((((uint16_t)(r) & 0xF8) << 8) | \
               (((uint16_t)(g) & 0xFC) << 3) | \
               (((uint16_t)(b) & 0xF8) >> 3))

/* LCD的长宽定义 */
#define DRVP_LCD_WIGTH    240
#define DRVP_LCD_LENGTH   320

void drvp_fmc_lcd_init(void);
uint32_t drvp_fmc_lcd_readid(void);
void drvp_fmc_lcd_display_ctrl(bool isopen);
void drvp_fmc_lcd_set_axis_scan(uint8_t my,uint8_t mx,uint8_t mv,uint8_t ml,uint8_t mh);
void drvp_fmc_lcd_clear(void);

void drvp_fmc_lcd_set_wid(uint16_t xstart,uint16_t xstop,
	                        uint16_t ystart,uint16_t ystop);
void drvp_fmc_lcd_writebuf(uint16_t* wbuf,uint32_t size);
void drvp_fmc_lcd_writesigle(uint16_t signlecolor,uint32_t size);
void drvp_fmc_lcd_readbuf(uint16_t* rbuf,uint32_t size);

#endif
