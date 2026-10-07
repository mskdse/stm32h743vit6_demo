#include "ltdc_lcd_disp.h"
#include "ltdc_front.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#define DISP_W   (LCD_WIN_X1 - LCD_WIN_X0)
#define DISP_H   (LCD_WIN_Y1 - LCD_WIN_Y0)

extern uint16_t LTDC_DISPLAY[(LCD_WIN_X1 * LCD_WIN_Y1)]; 

/* 文件指针 */
__attribute__((section(".RAM_D1"), aligned(4))) static FIL  fil;

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

/* 计算 BMP 一行占用的字节数（4 字节对齐） */
static int bmp_row_bytes(int bpp, int width)
{
    switch (bpp) {
    case 1:  return ((width + 7) / 8 + 3) & ~3;
    case 4:  return ((width + 1) / 2 + 3) & ~3;
    case 8:  return (width + 3) & ~3;
    case 16: return (width * 2 + 3) & ~3;
    case 24: return (width * 3 + 3) & ~3;
    case 32: return width * 4;
    default: return -1;
    }
}

/* 解码一行 BMP 像素到 RGB888 缓冲 */
static int decode_bmp_line(FIL *fil, int bpp, int width,
                           const uint8_t *palette, int pal_count,
                           uint8_t *rgb_line, uint8_t *raw_buf, int raw_size)
{
    int row_bytes = bmp_row_bytes(bpp, width);
    if (row_bytes < 0 || row_bytes > raw_size) return -1;

    UINT br;
    if (f_read(fil, raw_buf, row_bytes, &br) != FR_OK || br != (UINT)row_bytes)
        return -1;

    for (int x = 0; x < width; x++) {
        uint8_t r, g, b;
        switch (bpp) {
        case 1: {
            int idx = (raw_buf[x / 8] >> (7 - (x % 8))) & 1;
            if (idx >= pal_count) idx = 0;
            r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
            break;
        }
        case 4: {
            int byte_idx = x / 2;
            int idx = (x % 2 == 0) ? (raw_buf[byte_idx] >> 4) : (raw_buf[byte_idx] & 0x0F);
            if (idx >= pal_count) idx = 0;
            r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
            break;
        }
        case 8: {
            int idx = raw_buf[x];
            if (idx >= pal_count) idx = 0;
            r = palette[idx*4+2]; g = palette[idx*4+1]; b = palette[idx*4+0];
            break;
        }
        case 16: {
            uint16_t v = raw_buf[x*2] | (raw_buf[x*2+1] << 8);
            r = ((v >> 10) & 0x1F) << 3;
            g = ((v >>  5) & 0x1F) << 3;
            b = ( v        & 0x1F) << 3;
            break;
        }
        case 24: {
            b = raw_buf[x*3+0]; g = raw_buf[x*3+1]; r = raw_buf[x*3+2];
            break;
        }
        case 32: {
            b = raw_buf[x*4+0]; g = raw_buf[x*4+1]; r = raw_buf[x*4+2];
            break;
        }
        default: return -1;
        }
        rgb_line[x*3+0] = r;
        rgb_line[x*3+1] = g;
        rgb_line[x*3+2] = b;
    }
    return 0;
}

/*
 * 显示 BMP 文件，强制缩放到 DISP_W x DISP_H，铺满整个显存
 * path: FatFs 路径，如 "1:/test.bmp"
 * 返回 0 成功，负数失败
 */
int ltdc_lcd_disp_bmp(const char *path)
{
    FRESULT fr;
    UINT br;
    BMP_FILE_HEADER fh;
    BMP_INFO_HEADER ih;

    fr = f_open(&fil, path, FA_READ);
    if (fr != FR_OK) return -1;

    fr = f_read(&fil, &fh, sizeof(fh), &br);
    if (fr != FR_OK || br != sizeof(fh)) { f_close(&fil); return -2; }
    if (fh.bfType != 0x4D42)             { f_close(&fil); return -3; }

    fr = f_read(&fil, &ih, sizeof(ih), &br);
    if (fr != FR_OK || br != sizeof(ih))                { f_close(&fil); return -4; }
    if (ih.biCompression != 0 && ih.biCompression != 3) { f_close(&fil); return -5; }

    int bpp = ih.biBitCount;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) {
        f_close(&fil); return -6;
    }

    int src_w = ih.biWidth;
    int src_h = ih.biHeight;
    int top_down = 0;
    if (src_h < 0)                { src_h = -src_h; top_down = 1; }
    if (src_w <= 0 || src_h <= 0) { f_close(&fil); return -7; }

    /* 调色板 */
    uint8_t palette[256 * 4];
    int pal_count = 0;
    if (bpp <= 8) {
        pal_count = ih.biClrUsed ? ih.biClrUsed : (1 << bpp);
        if (pal_count > 256) pal_count = 256;
        fr = f_read(&fil, palette, pal_count * 4, &br);
        if (fr != FR_OK || br != (UINT)(pal_count * 4)) { f_close(&fil); return -8; }
    }

    /* 分配缓冲 */
    int src_row_bytes = bmp_row_bytes(bpp, src_w);
    uint8_t *rgb_line = (uint8_t *)sram_d2_malloc(src_w * 3);
    uint8_t *raw_buf  = (uint8_t *)sram_d2_malloc(src_row_bytes);
    if (!rgb_line || !raw_buf) {
        sram_d2_free(rgb_line);
        sram_d2_free(raw_buf);
        f_close(&fil); return -10;
    }

    /* ========== 按目标行循环，保证每个目标行都被精确写入一次 ========== */
    for (int dst_y = 0; dst_y < DISP_H; dst_y++) {

        /* 目标行 dst_y 对应图像真实行 real_src_y（0 在顶部） */
        int real_src_y = (dst_y * src_h) / DISP_H;
        if (real_src_y >= src_h) real_src_y = src_h - 1;

        /* 图像真实行对应文件中的行号（BMP 默认自下而上） */
        int file_src_y = top_down ? real_src_y : (src_h - 1 - real_src_y);

        /* 定位到该行在文件中的位置 */
        FSIZE_t offset = fh.bfOffBits + (FSIZE_t)file_src_y * src_row_bytes;
        fr = f_lseek(&fil, offset);
        if (fr != FR_OK) {
            sram_d2_free(rgb_line);
            sram_d2_free(raw_buf);
            f_close(&fil); return -12;
        }

        /* 读一行并解码成 RGB888 */
        if (decode_bmp_line(&fil, bpp, src_w, palette, pal_count,
                            rgb_line, raw_buf, src_row_bytes) != 0) {
            sram_d2_free(rgb_line);
            sram_d2_free(raw_buf);
            f_close(&fil); return -13;
        }

        /* 写入目标行，水平方向缩放 */
        uint16_t *dst = &LTDC_DISPLAY[dst_y * DISP_W];
        for (int dst_x = 0; dst_x < DISP_W; dst_x++) {
            int src_x = (dst_x * src_w) / DISP_W;
            if (src_x >= src_w) src_x = src_w - 1;
            dst[dst_x] = rgb888_to_565(rgb_line[src_x*3+0],
                                       rgb_line[src_x*3+1],
                                       rgb_line[src_x*3+2]);
        }
    }
    /* ================================================================ */

    sram_d2_free(rgb_line);
    sram_d2_free(raw_buf);
    f_close(&fil);
    return 0;
}

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
