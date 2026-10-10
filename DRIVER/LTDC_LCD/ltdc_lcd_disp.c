#include "ltdc_lcd_disp.h"
#include "ltdc_front.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#define DISP_W   (LCD_WIN_X1 - LCD_WIN_X0)
#define DISP_H   (LCD_WIN_Y1 - LCD_WIN_Y0)

extern uint16_t LTDC_DISPLAY[((LCD_WIN_X1-LCD_WIN_X0)*(LCD_WIN_Y1-LCD_WIN_Y0))]; 

/* 可变参函数的缓冲区地址 */
__attribute__((section(".RAM_D1")))             static char lcd_print_buf[256];

/* 常量数组，用来挂载各类字库
   front_index_add表示显示一行后数组索引的单位内需要偏移几个字节
   disp_rows表示行数
   disp_colums表示列数
	 bytes_per_char表示每字占用数组的总字节数（含前缀1/2字节）
	 front_size表示数组的总字节个数
*/
#define ASCII_BYTES(r, bpl)     (1 + (r) * (bpl))
#define CHINSES_BYTES(r, bpl)   (2 + (r) * (bpl))

const ltdc_front_postion_type ltdc_front_postion[FRONT_COUNT]=
{
    // ASCII：前缀 1 字节
    {.front=ASCLL_6X12_FORNT,    .front_index_add=1, .disp_rows=12, .disp_colums=6,
     .bytes_per_char=ASCII_BYTES(12, 1), .front_size=sizeof(ASCLL_6X12_FORNT)
		},

    {.front=ASCLL_8X16_FORNT,    .front_index_add=1, .disp_rows=16, .disp_colums=8,
     .bytes_per_char=ASCII_BYTES(16, 1), .front_size=sizeof(ASCLL_8X16_FORNT)
		},

    {.front=ASCLL_12X24_FORNT,   .front_index_add=2, .disp_rows=24, .disp_colums=12,
     .bytes_per_char=ASCII_BYTES(24, 2), .front_size=sizeof(ASCLL_12X24_FORNT)
		},

    {.front=ASCLL_16X32_FORNT,   .front_index_add=2, .disp_rows=32, .disp_colums=16,
     .bytes_per_char=ASCII_BYTES(32, 2), .front_size=sizeof(ASCLL_16X32_FORNT)
		},

    // 汉字：前缀 2 字节
    {.front=CHINSES_11X12_FORNT, .front_index_add=2, .disp_rows=12, .disp_colums=11,
     .bytes_per_char=CHINSES_BYTES(12, 2), .front_size=sizeof(CHINSES_11X12_FORNT)
		},

    {.front=CHINSES_15X16_FORNT, .front_index_add=2, .disp_rows=16, .disp_colums=15,
     .bytes_per_char=CHINSES_BYTES(16, 2), .front_size=sizeof(CHINSES_15X16_FORNT)
		},

    {.front=CHINSES_24X24_FORNT, .front_index_add=3, .disp_rows=24, .disp_colums=24,
     .bytes_per_char=CHINSES_BYTES(24, 3), .front_size=sizeof(CHINSES_24X24_FORNT)
		},

    {.front=CHINSES_32X32_FORNT, .front_index_add=4, .disp_rows=32, .disp_colums=32,
     .bytes_per_char=CHINSES_BYTES(32, 4), .front_size=sizeof(CHINSES_32X32_FORNT)
		},
};

/**
 * @brief  根据字体类型和编码计算字模在数组中的起始索引
 * @param  front  字体类型
 * @param  code   ASCII 用低 8 位，汉字用 16 位 GB2312 编码
 * @retval 字模第一个字节在数组中的索引；找不到返回 0xFFFFFFFF
 */
static uint32_t ltdc_lcd_get_pos(ltdc_front_type front, uint16_t code)
{
    const ltdc_front_postion_type* pos = &ltdc_front_postion[front];

    if (front < CHINSES_11X12)
    {
        // ===== ASCII：公式计算 =====
        uint8_t ch = (uint8_t)(code & 0xFF);
        if (ch < 0x01 || ch > 0x7F) return 0xFFFFFFFF;

        // (ch - 1) * 每字字节 + 1（跳过 1 字节前缀）
        return (uint32_t)(ch - 1) * pos->bytes_per_char + 1;
    }
    else
    {
        // ===== 汉字：遍历查找 =====
        uint8_t high = (code >> 8) & 0xFF;
        uint8_t low  = code & 0xFF;

        if (high < 0xA1 || high > 0xF7) return 0xFFFFFFFF;
        if (low  < 0xA1 || low  > 0xFE) return 0xFFFFFFFF;

        uint32_t total = pos->front_size / pos->bytes_per_char;
        uint32_t base;
			
        for (uint32_t i = 0; i < total; i++)
        {
            base = i * pos->bytes_per_char;
            if (pos->front[base] == high && pos->front[base + 1] == low)
            {
                return base + 2;   // 跳过 2 字节编码
            }
        }
        return 0xFFFFFFFF;
    }
}

static void ltdc_lcd_disp_single(uint16_t xp,uint16_t yp,
	                               uint16_t ch_asc,ltdc_front_type front,
										             uint16_t defalut_color,uint16_t disp_color)
{
    ltdc_lcd_ch_type               lcd_ch;
	  const ltdc_front_postion_type* lcd_pos=&ltdc_front_postion[front];
	  uint8_t                        byte;
    
	  /* 如果ch_asc是ASCLL码那么根据front枚举参数找到对应数组
	     的第一个有效字节的开始的索引，如果是汉字则找汉字
  	*/
    lcd_ch.front_index=ltdc_lcd_get_pos(front, ch_asc);
    if(lcd_ch.front_index == 0xFFFFFFFF) return;

    lcd_ch.disp=yp*DISP_W + xp;

    for (int i = 0; i < lcd_pos->disp_rows; i++)
    {
        for (int j = 0; j < lcd_pos->disp_colums; j++)
        {
            byte = lcd_pos->front[lcd_ch.front_index + (j >> 3)];
            if (byte & (0x80 >> (j & 7))) LTDC_DISPLAY[lcd_ch.disp] = disp_color;
            else                          LTDC_DISPLAY[lcd_ch.disp] = defalut_color;
            lcd_ch.disp++;
        }
        lcd_ch.front_index += lcd_pos->front_index_add;
        lcd_ch.disp        += (DISP_W - lcd_pos->disp_colums);
    }
}

void ltdc_lcd_disp_str(uint16_t xp,uint16_t yp,const char* str,
	                     ltdc_front_type asc_front,ltdc_front_type ch_front,
										   uint16_t defalut_color,uint16_t disp_color)
{
	while (*str)
	{
		if ((unsigned char)*str >= 0x80)//说明是汉字，需要提取两个字节
		{
			ltdc_lcd_disp_single(xp,yp,((str[0]<<8)|str[1]),ch_front,defalut_color,disp_color);
			xp+=ltdc_front_postion[ch_front].disp_colums;
			str+=2;
		}
		else
		{
			ltdc_lcd_disp_single(xp,yp,str[0],asc_front,defalut_color,disp_color);
			xp+=ltdc_front_postion[asc_front].disp_colums;
			str+=1;
		}
	}
}

void ltdc_lcd_disp_print(uint16_t xp, uint16_t yp,
                         ltdc_front_type asc_front,ltdc_front_type ch_front,
                         uint16_t defalut_color, uint16_t disp_color,
                         const char* fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    vsnprintf(lcd_print_buf,sizeof(lcd_print_buf),fmt,args);
    va_end(args);
	
    ltdc_lcd_disp_str(xp,yp,lcd_print_buf,asc_front,ch_front,defalut_color,disp_color);
}
