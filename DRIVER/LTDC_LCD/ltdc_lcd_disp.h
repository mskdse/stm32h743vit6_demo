#ifndef _LTDC_LCD_DISP_H_
#define _LTDC_LCD_DISP_H_

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

/* 以下是显示字符相关 */
typedef enum
{
	ASCLL_6X12,
	ASCLL_8X16,
	ASCLL_12X24,
	ASCLL_16X32,
	CHINSES_11X12,
	CHINSES_15X16,
	CHINSES_24X24,
	CHINSES_32X32,
	FRONT_COUNT
}ltdc_front_type;

typedef struct
{
	const unsigned char* front;//常量指针，用来指向字库
	uint8_t  front_index_add;//字库数组的索引递增个数
	uint8_t  disp_rows;//显示行数
	uint8_t  disp_colums;//显示列数
}ltdc_front_postion_type;

typedef struct
{
	uint32_t front_index;//字库数组的起始索引
	uint32_t disp;//显示索引
}ltdc_lcd_ch_type;

/* 函数声明 */							
void ltdc_lcd_disp_str(uint16_t xp,uint16_t yp,
	                     const char* str,ltdc_front_type front,
										   uint16_t defalut_color,uint16_t disp_color);
											 
void ltdc_lcd_disp_print(uint16_t xp, uint16_t yp,
                         ltdc_front_type front,
                         uint16_t defalut_color, uint16_t disp_color,
                         const char* fmt, ...);

int  ltdc_lcd_disp_bmp(const char *path);

#endif
