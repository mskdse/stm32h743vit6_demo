#ifndef _LTDC_LCD_DISP_H_
#define _LTDC_LCD_DISP_H_

#include "ltdc_lcd.h"

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
	uint32_t bytes_per_char;   // 每字总字节数（含前缀）
  uint32_t front_size;       // 字库总字节数
}ltdc_front_postion_type;

typedef struct
{
	uint32_t front_index;//字库数组的起始索引
	uint32_t disp;//显示索引
}ltdc_lcd_ch_type;

/* 函数声明 */							
void ltdc_lcd_disp_str(uint16_t xp,uint16_t yp,const char* str,
	                     ltdc_front_type asc_front,ltdc_front_type ch_front,
										   uint16_t defalut_color,uint16_t disp_color);
											 
void ltdc_lcd_disp_print(uint16_t xp, uint16_t yp,
                         ltdc_front_type asc_front,ltdc_front_type ch_front,
                         uint16_t defalut_color, uint16_t disp_color,
                         const char* fmt, ...);

#endif
