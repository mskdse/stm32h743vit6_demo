/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <stdbool.h>

/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
    #define MY_DISP_HOR_RES    320
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen height, default value 240 is used for now.
    #define MY_DISP_VER_RES    240
#endif

/* DMA最大单次传输长度（根据你的DMA配置，通常是16位传输） */
#define DMA_MAX_TRANSFER_LEN    65535

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    lv_disp_drv_t *disp_drv;
    const lv_area_t *area;
    lv_color_t *color_p;
    uint32_t total_pixels;
    uint32_t sent_pixels;
    bool is_busy;
} disp_flush_ctx_t;

static void dma_transfer_next_chunk(void);
static disp_flush_ctx_t flush_ctx = {0};

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//        const lv_area_t * fill_area, lv_color_t color);

void lv_port_disp_init(void)
{
    disp_init();

//    /* Example for 1) */
//    static lv_disp_draw_buf_t draw_buf_dsc_1;
//    static lv_color_t buf_1[MY_DISP_HOR_RES * 10];                          /*A buffer for 10 rows*/
//    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 10);   /*Initialize the display buffer*/

//    /* Example for 2) */
//    static lv_disp_draw_buf_t draw_buf_dsc_2;
//    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 10];                        /*A buffer for 10 rows*/
//    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 10];                        /*An other buffer for 10 rows*/
//    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 10);   /*Initialize the display buffer*/

    /* Example for 3) also set disp_drv.full_refresh = 1 below*/
    static lv_disp_draw_buf_t draw_buf_dsc_3;
    __attribute__((section (".RAM_D1"),used))static lv_color_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES];            
    __attribute__((section (".RAM_D1"),used))static lv_color_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES];      
    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,MY_DISP_VER_RES * MY_DISP_HOR_RES);

    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc_3;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* DMA全满中断，由于我一次传输可能超过65535的最大限制，所以让ai写一个DMA分块传输 */
static void dma_comptle_callback(DMA_HandleTypeDef *_hdma)
{
	if(DMA1_Stream1==_hdma->Instance)
	{
		/* 更新已发送像素计数 */
		uint32_t chunk_size = flush_ctx.total_pixels - flush_ctx.sent_pixels;

		if (chunk_size > DMA_MAX_TRANSFER_LEN) chunk_size = DMA_MAX_TRANSFER_LEN;

		flush_ctx.sent_pixels += chunk_size;

		/* 检查是否所有数据都已发送完成 */
		if (flush_ctx.sent_pixels >= flush_ctx.total_pixels) 
		{
				/* 所有数据传输完成，通知LVGL */
				flush_ctx.is_busy = false;
				lv_disp_flush_ready(flush_ctx.disp_drv);
		} 
		else dma_transfer_next_chunk();/* 还有数据未传输，继续下一块 */
	}
}
/* 启动下一块DMA传输 */
static void dma_transfer_next_chunk(void)
{
    uint32_t remaining = flush_ctx.total_pixels - flush_ctx.sent_pixels;
    uint32_t chunk_size = (remaining > DMA_MAX_TRANSFER_LEN) ? DMA_MAX_TRANSFER_LEN : remaining;
    
    /* 计算当前要发送的数据起始地址 */
    lv_color_t *current_color_p = flush_ctx.color_p + flush_ctx.sent_pixels;
    
    /* 注意：这里需要根据你的LCD窗口设置来计算实际的起始坐标偏移 */
    /* 由于是全屏刷新，且开启了full_refresh，LVGL会一次性提供整个屏幕的数据 */
    /* 但我们分块传输时，需要确保LCD窗口跟随数据块的位置 */
    
    /* 计算当前块对应的窗口坐标 */
    uint32_t pixels_per_row = MY_DISP_HOR_RES;
    uint32_t start_pixel = flush_ctx.sent_pixels;
    uint16_t start_x = start_pixel % pixels_per_row;
    uint16_t start_y = start_pixel / pixels_per_row;
    uint16_t end_pixel = flush_ctx.sent_pixels + chunk_size - 1;
    uint16_t end_x = end_pixel % pixels_per_row;
    uint16_t end_y = end_pixel / pixels_per_row;
    
    /* 设置LCD窗口为当前块的范围 */
    drvp_fmc_lcd_set_wid(start_x, end_x, start_y, end_y);
    
    /* 启动DMA传输当前块 */
    drv_fmc_dma_m2m_tranf((uint16_t*)current_color_p, chunk_size);
}

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
	  drv_fmc_dma_m2m_init(dma_comptle_callback);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
//    if(disp_flush_enabled) 
//	 {
//        /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
////        int32_t x;
////        int32_t y;
////        for(y = area->y1; y <= area->y2; y++) {
////            for(x = area->x1; x <= area->x2; x++) {
////                /*Put a pixel to the display. For example:*/
////                /*put_px(x, y, *color_p)*/
////                color_p++;
////            }
////        }

////		 drvp_fmc_lcd_set_wid(area->x1,area->x2,area->y1,area->y2);
////		 drvp_fmc_lcd_writebuf((uint16_t*)color_p,((area->x2-area->x1+1)*(area->y2-area->y1+1)));
//    }

//    /*IMPORTANT!!!
//     *Inform the graphics library that you are ready with the flushing*/
//    lv_disp_flush_ready(disp_drv);
	
	   if(!disp_flush_enabled) 
	  {
		  lv_disp_flush_ready(disp_drv);
		  return;
    }

    /* 如果上一次DMA传输还未完成，理论上不应该发生（LVGL会等待flush_ready） */
    if (flush_ctx.is_busy) while (flush_ctx.is_busy);/* 安全保护：等待上一次完成（实际项目中可加超时处理） */

    /* 计算总像素数 */
    uint32_t total_pixels = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    
    /* 保存上下文供DMA回调使用 */
    flush_ctx.disp_drv = disp_drv;
    flush_ctx.area = area;
    flush_ctx.color_p = color_p;
    flush_ctx.total_pixels = total_pixels;
    flush_ctx.sent_pixels = 0;
    flush_ctx.is_busy = true;

    /* 启动第一块DMA传输 */
    dma_transfer_next_chunk();
}

/*OPTIONAL: GPU INTERFACE*/

/*If your MCU has hardware accelerator (GPU) then you can use it to fill a memory with a color*/
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                    const lv_area_t * fill_area, lv_color_t color)
//{
//    /*It's an example code which should be done by your GPU*/
//    int32_t x, y;
//    dest_buf += dest_width * fill_area->y1; /*Go to the first line*/
//
//    for(y = fill_area->y1; y <= fill_area->y2; y++) {
//        for(x = fill_area->x1; x <= fill_area->x2; x++) {
//            dest_buf[x] = color;
//        }
//        dest_buf+=dest_width;    /*Go to the next line*/
//    }
//}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
