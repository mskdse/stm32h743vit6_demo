/**
 * @file lv_port_disp.c
 */

#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <stdbool.h>
#include "ltdc_lcd.h"

extern uint16_t LTDC_DISPLAY[((LCD_WIN_X1-LCD_WIN_X0)*(LCD_WIN_Y1-LCD_WIN_Y0))];

/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #define MY_DISP_HOR_RES    (LCD_WIN_X1-LCD_WIN_X0)
#endif

#ifndef MY_DISP_VER_RES
    #define MY_DISP_VER_RES    (LCD_WIN_Y1-LCD_WIN_Y0)
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_disp_draw_buf_t draw_buf_dsc_1;
__attribute__((section(".RAM_D1")))static lv_color_t buf_1_1[MY_DISP_HOR_RES*30];
static lv_disp_drv_t disp_drv;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Register the display in LVGL
     * -----------------------------------*/

    /* 1. 初始化绘制缓冲区 */
    lv_disp_draw_buf_init(&draw_buf_dsc_1,
                          buf_1_1,      // buf1
                          NULL,         // buf2
                          MY_DISP_HOR_RES * 30);

    /* 2. 初始化显示驱动 */
    lv_disp_drv_init(&disp_drv);

    /* 3. 设置分辨率 */
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;

    /* 4. 设置刷新回调 */
    disp_drv.flush_cb = disp_flush;

    /* 5. 绑定绘制缓冲区 */
    disp_drv.draw_buf = &draw_buf_dsc_1;

    /* 6. 注册驱动 */
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void disp_init(void)
{
    /* 你的 LCD 初始化代码，通常已经在 main 里做过，这里留空即可 */
}

volatile bool disp_flush_enabled = true;

void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/* 刷新回调：把 LVGL 渲染好的像素搬运到 LTDC_DISPLAY */
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if (disp_flush_enabled)
    {
/* 1.阻塞式写buffer，速度比较慢 */
//        int32_t x;
//        int32_t y;

//        for (y = area->y1; y <= area->y2; y++)
//        {
//            for (x = area->x1; x <= area->x2; x++)
//            {
//                LTDC_DISPLAY[y * LCD_WIN_X1 + x] = color_p->full;
//                color_p++;
//            }
//        }
			
/* 2.使用DMA2D进行渲染，虽然也是阻塞式但是速度快 */
				ltdc_lcd_dma2d_data_copy((const uint16_t *)color_p,area->x1,(area->x2-area->x1+1),
					                                                 area->y1,(area->y2-area->y1+1));
    }

    lv_disp_flush_ready(disp_drv);
}

#else
typedef int keep_pedantic_happy;
#endif
