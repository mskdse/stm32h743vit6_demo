#ifndef _LTDC_LCD_DISP_BMP_H_
#define _LTDC_LCD_DISP_BMP_H_

#include "ltdc_lcd.h"
#include "sram_d2_malloc.h"
#include "ff.h"

/* 以下是跟显示BMP图片相关，图片需要存放到SD卡 */
#pragma pack(push, 1)
typedef struct 
{
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BMP_FILE_HEADER;

typedef struct 
{
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BMP_INFO_HEADER;
#pragma pack(pop)

static inline uint16_t rgb888_to_565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int  ltdc_lcd_disp_bmp(const char *path);

#endif
