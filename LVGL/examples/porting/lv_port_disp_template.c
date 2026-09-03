/**
 * @file lv_port_disp_templ.c
 *
 * True double-buffered display port for an 8080/FMC LCD, pushed by one DMA1
 * memory-to-memory stream.
 *
 * Strategy
 * --------
 * Two PARTIAL frame buffers (DISP_BUF_ROWS rows each) are registered with LVGL.
 * LVGL renders a strip of rows into buffer A, then disp_flush() starts an
 * asynchronous DMA burst that pushes A to the panel and returns immediately.
 * While the DMA is still sending A, LVGL renders the next strip into buffer B
 * -> CPU drawing and panel transfer run in parallel. lv_disp_flush_ready() is
 * called from the DMA complete interrupt, i.e. as soon as the flushed strip is
 * really on the LCD.
 *
 * Why partial buffers (not full screen):
 * With FULL-screen double buffers LVGL (v8, non-direct mode) waits at the start
 * of a frame until the previous whole-frame flush has finished before it starts
 * rendering again, so DMA and rendering are serialized -> slower. Partial double
 * buffers are the only configuration where LVGL alternates buffers and lets the
 * next strip be rendered while the previous one is still being DMAed.
 *
 * Each single flush carries at most DISP_BUF_ROWS * HOR_RES pixels, which is
 * kept <= DMA_MAX_TRANSFER_LEN, so a flush always fits in ONE DMA burst and no
 * chunking / window-splitting is required.
 */
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"
#include <stdbool.h>
#include "drvp_fmc_lcd.h"   /* drvp_fmc_lcd_set_wid */
#include "drv_fmc_lcd.h"    /* drv_fmc_dma_m2m_init / drv_fmc_dma_m2m_tranf */

/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
    #define MY_DISP_HOR_RES    320
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
    #define MY_DISP_VER_RES    240
#endif

/* Maximum length of one DMA memory-to-memory burst.
 * The H7 DMA transfer counter (NDT) is 16-bit, so a single burst can move at most
 * 65535 half-words (= 65535 RGB565 pixels). */
#define DMA_MAX_TRANSFER_LEN    65535

/* Rows of one partial draw buffer. A whole flush is DISP_BUF_ROWS x HOR_RES
 * pixels, so it must stay <= DMA_MAX_TRANSFER_LEN for a single DMA burst.
 * A divisor of the screen height gives LVGL strips of equal size. */
#define DISP_BUF_ROWS           (MY_DISP_VER_RES / 2)

#if (MY_DISP_HOR_RES * DISP_BUF_ROWS) > DMA_MAX_TRANSFER_LEN
    #error "One flush would exceed the DMA burst limit: use smaller DISP_BUF_ROWS"
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
/* Driver to signal when the DMA of a flushed strip has finished.
 * Assigned in disp_flush() before starting the transfer. */
static lv_disp_drv_t * flush_drv;

void lv_port_disp_init(void)
{
    disp_init();

    /* Two partial double buffers. They are placed in AXI-SRAM (.RAM_D1), which
     * this project maps as non-cacheable, so DMA reads of them are coherent. */
    static lv_disp_draw_buf_t draw_buf_dsc;
    __attribute__((section (".RAM_D1"),used))static lv_color_t buf_1[MY_DISP_HOR_RES * DISP_BUF_ROWS];
    __attribute__((section (".RAM_D1"),used))static lv_color_t buf_2[MY_DISP_HOR_RES * DISP_BUF_ROWS];
    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, MY_DISP_HOR_RES * DISP_BUF_ROWS);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;

    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    /* NOTE: full_refresh is intentionally NOT set. With full-screen buffers LVGL
     * serializes render + DMA; partial buffers are what enable the overlap. */

    lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* DMA complete interrupt callback (called by HAL from DMA1_Stream1_IRQHandler).
 * The strip handed to the DMA in disp_flush() is now on the panel -> let LVGL
 * release that buffer and keep going. */
static void dma_cplt_callback(DMA_HandleTypeDef *hdma)
{
    lv_disp_flush_ready(flush_drv);
}

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    drv_fmc_dma_m2m_init(dma_cplt_callback);
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

/*Flush the content of the internal buffer the specific area on the display.
 *The flush area is always a strip of at most DISP_BUF_ROWS rows, i.e. at most
 *DMA_MAX_TRANSFER_LEN pixels, so it maps 1:1 onto the GRAM window set below and
 *fits into a single DMA burst. Return immediately and call 'lv_disp_flush_ready()'
 *from the DMA interrupt: LVGL then renders the next strip into the other buffer
 *while this one is being transferred. (lv_disp_flush_ready must NOT be called
 *synchronously here, otherwise LVGL cannot overlap drawing with the DMA.)*/
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(!disp_flush_enabled) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    flush_drv = disp_drv;

    drvp_fmc_lcd_set_wid((uint16_t)area->x1, (uint16_t)area->x2,
                         (uint16_t)area->y1, (uint16_t)area->y2);
    drv_fmc_dma_m2m_tranf((uint16_t *)color_p,
                          (uint16_t)((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)));
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
