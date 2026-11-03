#include "avilib_app.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#ifndef AVI_APP_TJPGD_HEADER
#define AVI_APP_TJPGD_HEADER "tjpgd.h"
#endif
#include AVI_APP_TJPGD_HEADER

#ifdef AVI_APP_LCD_HEADER
#include AVI_APP_LCD_HEADER
#else
extern void drvp_fmc_lcd_set_wid(uint16_t x1, uint16_t x2,
                                uint16_t y1, uint16_t y2);
extern void drvp_fmc_lcd_writebuf(uint16_t *buf, uint16_t pixel_count);
#endif

#ifndef AVI_APP_TASK_STACK_SIZE
#define AVI_APP_TASK_STACK_SIZE       1024U /* FreeRTOS stack words, not bytes */
#endif
#ifndef AVI_APP_TASK_PRIORITY
#define AVI_APP_TASK_PRIORITY         (tskIDLE_PRIORITY + 2U)
#endif
#ifndef AVI_APP_JPEG_WORK_SIZE
#define AVI_APP_JPEG_WORK_SIZE        4096U
#endif

#define AVI_APP_NOTIFY_TICK           (1UL << 0)
#define AVI_APP_NOTIFY_EXIT           (1UL << 1)
#define AVI_APP_DEFAULT_FPS           25.0

/* All large / persistent objects used by the player are placed in D1 SRAM. */
typedef struct {
    avi_t *avi;
    TaskHandle_t task;
    TimerHandle_t timer;
    SemaphoreHandle_t lock;
    SemaphoreHandle_t exit_sem;
    volatile AVI_APP_STATE state;
    volatile uint8_t exit_requested;
    uint32_t current_frame;
    AVI_APP_SPEED speed;
    AVI_APP_JPEG_SCALE jpeg_scale;
    AVI_APP_LOOP_MODE loop_mode;
    uint16_t display_x;
    uint16_t display_y;
    AVI_APP_AUDIO_HOOK audio_hook;
    void *audio_user;
    AVI_APP_INFO info;
} avi_app_context_t;

typedef struct {
    FIL *file;
    FSIZE_t frame_pos;
    uint32_t frame_len;
    uint32_t logical_pos;
    uint32_t insert_pos;
    uint8_t insert_dht;
} avi_app_jpeg_source_t;

__attribute__((section(".RAM_D1"), aligned(4))) static avi_app_context_t g_avi_app;
__attribute__((section(".RAM_D1"), aligned(4))) static JDEC g_avi_jdec;
__attribute__((section(".RAM_D1"), aligned(4))) static uint8_t g_avi_jpeg_work[AVI_APP_JPEG_WORK_SIZE];

static const uint8_t g_speed_num[] = { 1, 1, 3, 1, 5, 3, 2, 4 };
static const uint8_t g_speed_den[] = { 4, 2, 4, 1, 4, 2, 1, 1 };

static const uint8_t g_avi_mjpeg_dht[420] = {
    0xFF, 0xC4, 0x01, 0xA2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02,
    0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x01, 0x00, 0x03,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
    0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04,
    0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22,
    0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15,
    0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
    0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2,
    0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5,
    0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
    0xFA, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05,
    0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04,
    0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22,
    0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33,
    0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25,
    0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35, 0x36,
    0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
    0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA,
    0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA
};

static int avi_app_speed_valid(AVI_APP_SPEED speed)
{
    return ((unsigned)speed < (sizeof(g_speed_num) / sizeof(g_speed_num[0])));
}

static int avi_app_fourcc_equal(const char *a, const char *b)
{
    unsigned int i;
    if(a == NULL || b == NULL) return 0;
    for(i = 0; i < 4; i++) {
        char ca = a[i], cb = b[i];
        if(ca >= 'a' && ca <= 'z') ca = (char)(ca - ('a' - 'A'));
        if(cb >= 'a' && cb <= 'z') cb = (char)(cb - ('a' - 'A'));
        if(ca != cb) return 0;
    }
    return 1;
}

static int avi_app_is_mjpeg(const avi_t *avi)
{
    if(avi == NULL) return 0;
    return avi_app_fourcc_equal(avi->compressor2, "MJPG") ||
           avi_app_fourcc_equal(avi->compressor2, "JPEG") ||
           avi_app_fourcc_equal(avi->compressor2, "JPG ") ||
           avi_app_fourcc_equal(avi->compressor, "MJPG") ||
           avi_app_fourcc_equal(avi->compressor, "JPEG") ||
           avi_app_fourcc_equal(avi->compressor, "JPG ");
}

static const char *avi_app_video_fourcc(const avi_t *avi)
{
    if(avi == NULL) return "????";
    if(avi_app_fourcc_equal(avi->compressor2, "MJPG") ||
       avi_app_fourcc_equal(avi->compressor2, "JPEG") ||
       avi_app_fourcc_equal(avi->compressor2, "JPG ")) return avi->compressor2;
    if(avi_app_fourcc_equal(avi->compressor, "MJPG") ||
       avi_app_fourcc_equal(avi->compressor, "JPEG") ||
       avi_app_fourcc_equal(avi->compressor, "JPG ")) return avi->compressor;
    return avi->compressor2[0] ? avi->compressor2 : avi->compressor;
}

static uint32_t avi_app_frame_period_ms(void)
{
    double fps = AVI_frame_rate(g_avi_app.avi);
    double period;
    if(fps <= 0.0) fps = AVI_APP_DEFAULT_FPS;
    period = 1000.0 * (double)g_speed_den[g_avi_app.speed] /
             (fps * (double)g_speed_num[g_avi_app.speed]);
    if(period < 1.0) period = 1.0;
    if(period > 60000.0) period = 60000.0;
    return (uint32_t)(period + 0.5);
}

static TickType_t avi_app_timer_period(void)
{
    double fps = AVI_frame_rate(g_avi_app.avi);
    double ticks_d;
    TickType_t ticks;
    if(fps <= 0.0) fps = AVI_APP_DEFAULT_FPS;
    ticks_d = ((double)configTICK_RATE_HZ * (double)g_speed_den[g_avi_app.speed]) /
              (fps * (double)g_speed_num[g_avi_app.speed]);
    if(ticks_d < 1.0) ticks_d = 1.0;
    if(ticks_d > ((double)configTICK_RATE_HZ * 60.0))
        ticks_d = (double)configTICK_RATE_HZ * 60.0;
    ticks = (TickType_t)(ticks_d + 0.5);
    if(ticks == 0) ticks = 1;
    return ticks;
}

/*
 * Inspect JPEG marker segments up to SOS. AVI1/MJPG frames often omit DHT;
 * in that case the standard MJPEG Huffman tables are inserted after APP0.
 */
static int avi_app_scan_jpeg_header(FIL *file, FSIZE_t frame_pos,
                                    uint32_t frame_len, uint32_t *insert_pos,
                                    int *has_dht)
{
    uint8_t marker[2], length_bytes[2];
    UINT br;
    uint32_t p = 2;
    uint32_t seg_len;
    uint8_t m;

    if(file == NULL || insert_pos == NULL || has_dht == NULL || frame_len < 4U)
        return -1;

    if(f_lseek(file, frame_pos) != FR_OK ||
       f_read(file, marker, 2, &br) != FR_OK || br != 2U)
        return -1;
    if(marker[0] != 0xFF || marker[1] != 0xD8)
        return -1;

    *insert_pos = 2U;
    *has_dht = 0;

    while(p + 4U <= frame_len) {
        if(f_lseek(file, frame_pos + p) != FR_OK ||
           f_read(file, marker, 2, &br) != FR_OK || br != 2U)
            return -1;

        if(marker[0] != 0xFF)
            return -1;

        while(marker[1] == 0xFF) {
            p++;
            if(p + 4U > frame_len ||
               f_lseek(file, frame_pos + p) != FR_OK ||
               f_read(file, marker, 2, &br) != FR_OK || br != 2U)
                return -1;
            if(marker[0] != 0xFF)
                return -1;
        }

        m = marker[1];

        if(m == 0xDA || m == 0xD9)
            return 0;

        if(m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) {
            p += 2U;
            continue;
        }

        if(p + 4U > frame_len ||
           f_lseek(file, frame_pos + p + 2U) != FR_OK ||
           f_read(file, length_bytes, 2, &br) != FR_OK || br != 2U)
            return -1;

        seg_len = ((uint32_t)length_bytes[0] << 8) | length_bytes[1];
        if(seg_len < 2U || seg_len > frame_len - p - 2U)
            return -1;

        if(m == 0xC4) {
            *has_dht = 1;
            return 0;
        }

        if(p == 2U && m == 0xE0)
            *insert_pos = p + 2U + seg_len;

        p += 2U + seg_len;
    }

    return 0;
}

/* TJpgDec sees a virtual JPEG stream: original bytes plus optional DHT. */
static size_t avi_app_jpeg_in(JDEC *jd, uint8_t *buff, size_t nbyte)
{
    avi_app_jpeg_source_t *src = (avi_app_jpeg_source_t *)jd->device;
    uint32_t virtual_len;
    uint32_t available;
    size_t request;
    size_t done = 0;

    if(src == NULL || src->file == NULL || nbyte == 0U)
        return 0;

    virtual_len = src->frame_len +
                  (src->insert_dht ? (uint32_t)sizeof(g_avi_mjpeg_dht) : 0U);

    if(src->logical_pos > virtual_len)
        return 0;

    /*
     * Important: TJpgDec may ask for JD_SZBUF bytes when fewer bytes remain
     * in the current AVI chunk. The input callback must return the available
     * tail bytes, not fail the whole request. Returning 0 here caused JDR_INP
     * near the end of an otherwise valid MJPEG frame.
     */
    available = virtual_len - src->logical_pos;
    request = nbyte;
    if(request > (size_t)available)
        request = (size_t)available;
    if(request == 0U)
        return 0;

    if(buff == NULL) {
        src->logical_pos += (uint32_t)request;
        return request;
    }

    while(done < request) {
        uint32_t logical = src->logical_pos;
        uint32_t original_pos;
        uint32_t run;
        size_t take;

        if(src->insert_dht &&
           logical >= src->insert_pos &&
           logical < src->insert_pos + (uint32_t)sizeof(g_avi_mjpeg_dht)) {
            uint32_t dht_pos = logical - src->insert_pos;
            run = (uint32_t)sizeof(g_avi_mjpeg_dht) - dht_pos;
            take = request - done;
            if(take > (size_t)run)
                take = (size_t)run;
            memcpy(buff + done, g_avi_mjpeg_dht + dht_pos, take);
            src->logical_pos += (uint32_t)take;
            done += take;
        } else {
            if(src->insert_dht && logical < src->insert_pos) {
                original_pos = logical;
                run = src->insert_pos - logical;
            } else {
                original_pos = logical -
                    (src->insert_dht ? (uint32_t)sizeof(g_avi_mjpeg_dht) : 0U);
                if(original_pos >= src->frame_len)
                    break;
                run = src->frame_len - original_pos;
            }

            if(run == 0U)
                break;

            take = request - done;
            if(take > (size_t)run)
                take = (size_t)run;

            if(f_lseek(src->file, src->frame_pos + original_pos) != FR_OK)
                break;

            {
                UINT br = 0;
                FRESULT fr = f_read(src->file, buff + done, (UINT)take, &br);
                if(fr != FR_OK || br == 0U)
                    break;
                src->logical_pos += (uint32_t)br;
                done += br;
                if(br < take)
                    break;
            }
        }
    }

    return done;
}

/* TJpgDec RGB565 output callback: FMC window followed by pixel stream. */
static int avi_app_lcd_out(JDEC *jd, void *bitmap, JRECT *rect)
{
    uint32_t x1, x2, y1, y2, width, height, pixels;
    (void)jd;

    if(bitmap == NULL || rect == NULL) return 0;

    x1 = (uint32_t)rect->left + g_avi_app.display_x;
    x2 = (uint32_t)rect->right + g_avi_app.display_x;
    y1 = (uint32_t)rect->top + g_avi_app.display_y;
    y2 = (uint32_t)rect->bottom + g_avi_app.display_y;
    if(x2 > UINT16_MAX || y2 > UINT16_MAX || x1 > x2 || y1 > y2)
        return 0;

    width = x2 - x1 + 1U;
    height = y2 - y1 + 1U;
    pixels = width * height;
    if(pixels == 0U || pixels > UINT16_MAX)
        return 0;

    drvp_fmc_lcd_set_wid((uint16_t)x1, (uint16_t)x2,
                         (uint16_t)y1, (uint16_t)y2);
    drvp_fmc_lcd_writebuf((uint16_t *)bitmap, (uint16_t)pixels);
    return 1;
}

static AVI_APP_RESULT avi_app_decode_frame(uint32_t frame)
{
    avi_app_jpeg_source_t source;
    video_index_entry *entry;
    JRESULT jr;

    if(g_avi_app.avi == NULL || !g_avi_app.avi->index_ready)
        return AVI_APP_ERR_NO_INDEX;
    if(frame >= (uint32_t)g_avi_app.avi->video_frames)
        return AVI_APP_ERR_PARAM;

    entry = &g_avi_app.avi->video_index[frame];
    if(entry->len == 0U || entry->pos > g_avi_app.avi->file_size ||
       entry->len > g_avi_app.avi->file_size - entry->pos)
        return AVI_APP_ERR_FILE_READ;

    if(f_lseek(&g_avi_app.avi->fdes, (FSIZE_t)entry->pos) != FR_OK)
        return AVI_APP_ERR_FILE_READ;

    {
        int has_dht = 0;
        uint32_t insert_pos = 2U;

        source.file = &g_avi_app.avi->fdes;
        source.frame_pos = (FSIZE_t)entry->pos;
        source.frame_len = entry->len;
        source.logical_pos = 0U;
        source.insert_pos = 2U;
        source.insert_dht = 0U;

        if(avi_app_scan_jpeg_header(source.file, source.frame_pos,
                                    source.frame_len, &insert_pos,
                                    &has_dht) != 0) {
            printf("[AVI] Invalid JPEG marker stream, frame=%lu\r\n",
                   (unsigned long)frame);
            return AVI_APP_ERR_JPEG_PREPARE;
        }

        if(!has_dht) {
            source.insert_dht = 1U;
            source.insert_pos = insert_pos;
            if(frame == 0U)
                printf("[AVI] AVI1 MJPEG has no DHT, inject default Huffman tables\r\n");
        }
    }

    memset(&g_avi_jdec, 0, sizeof(g_avi_jdec));

    jr = jd_prepare(&g_avi_jdec, avi_app_jpeg_in, g_avi_jpeg_work,
                    sizeof(g_avi_jpeg_work), &source);
    if(jr != JDR_OK) {
        printf("[AVI] JPEG prepare failed, frame=%lu, err=%d, pos=%lu, left=%lu\r\n",
               (unsigned long)frame, (int)jr,
               (unsigned long)source.logical_pos,
               (unsigned long)(source.frame_len +
                   (source.insert_dht ? sizeof(g_avi_mjpeg_dht) : 0U) -
                   source.logical_pos));
        return AVI_APP_ERR_JPEG_PREPARE;
    }

    jr = jd_decomp(&g_avi_jdec, avi_app_lcd_out, (uint8_t)g_avi_app.jpeg_scale);
    if(jr != JDR_OK) {
        printf("[AVI] JPEG decode failed, frame=%lu, err=%d, input=%lu/%lu\r\n",
               (unsigned long)frame, (int)jr,
               (unsigned long)source.logical_pos,
               (unsigned long)(source.frame_len +
                   (source.insert_dht ? sizeof(g_avi_mjpeg_dht) : 0U)));
        return AVI_APP_ERR_JPEG_DECODE;
    }
    return AVI_APP_OK;
}

static void avi_app_timer_callback(TimerHandle_t timer)
{
    (void)timer;
    if(g_avi_app.task != NULL && g_avi_app.state == AVI_APP_STATE_PLAYING)
        xTaskNotify(g_avi_app.task, AVI_APP_NOTIFY_TICK, eSetBits);
}

static void avi_app_worker(void *argument)
{
    uint32_t notify;
    AVI_APP_RESULT result;
    uint32_t frame;
    (void)argument;

    for(;;) {
        notify = 0;
        xTaskNotifyWait(0U, UINT32_MAX, &notify, portMAX_DELAY);
        if((notify & AVI_APP_NOTIFY_EXIT) != 0U || g_avi_app.exit_requested)
            break;
        if((notify & AVI_APP_NOTIFY_TICK) == 0U ||
           g_avi_app.state != AVI_APP_STATE_PLAYING)
            continue;

        if(xSemaphoreTake(g_avi_app.lock, portMAX_DELAY) != pdTRUE)
            continue;

        if(g_avi_app.state != AVI_APP_STATE_PLAYING) {
            xSemaphoreGive(g_avi_app.lock);
            continue;
        }

        frame = g_avi_app.current_frame;
        result = avi_app_decode_frame(frame);
        if(result == AVI_APP_OK) {
            if(g_avi_app.state != AVI_APP_STATE_STOPPED &&
               g_avi_app.state != AVI_APP_STATE_CLOSED) {
                if(g_avi_app.audio_hook != NULL) {
                    g_avi_app.audio_hook(g_avi_app.avi, frame,
                                         avi_app_frame_period_ms(),
                                         g_avi_app.audio_user);
                }
                g_avi_app.current_frame++;
                AVI_set_video_position(g_avi_app.avi, (long)g_avi_app.current_frame);
                if(g_avi_app.current_frame >= (uint32_t)g_avi_app.avi->video_frames) {
                    if(g_avi_app.loop_mode == AVI_APP_LOOP_ENABLE) {
                        g_avi_app.current_frame = 0;
                        AVI_set_video_position(g_avi_app.avi, 0);
                    } else if(g_avi_app.state == AVI_APP_STATE_PLAYING) {
                        g_avi_app.state = AVI_APP_STATE_FINISHED;
                        xTimerStop(g_avi_app.timer, 0);
                    }
                }
            }
        } else if(g_avi_app.state != AVI_APP_STATE_STOPPED &&
                  g_avi_app.state != AVI_APP_STATE_CLOSED) {
            g_avi_app.state = AVI_APP_STATE_ERROR;
            xTimerStop(g_avi_app.timer, 0);
        }

        xSemaphoreGive(g_avi_app.lock);
    }

    g_avi_app.task = NULL;
    xSemaphoreGive(g_avi_app.exit_sem);
    vTaskDelete(NULL);
}

static void avi_app_print_info(const char *filename)
{
    avi_t *avi = g_avi_app.avi;
    const char *fourcc = avi_app_video_fourcc(avi);
    int audio_count = AVI_audio_tracks(avi);
    int i;

    memset(&g_avi_app.info, 0, sizeof(g_avi_app.info));
    g_avi_app.info.width = (uint32_t)AVI_video_width(avi);
    g_avi_app.info.height = (uint32_t)AVI_video_height(avi);
    g_avi_app.info.frame_count = (uint32_t)AVI_video_frames(avi);
    g_avi_app.info.max_frame_size = (uint32_t)AVI_max_video_chunk(avi);
    g_avi_app.info.fps_x1000 = (uint32_t)(AVI_frame_rate(avi) * 1000.0 + 0.5);
    g_avi_app.info.audio_track_count = (uint32_t)audio_count;
    if(fourcc != NULL) {
        memcpy(g_avi_app.info.video_fourcc, fourcc, 4);
        g_avi_app.info.video_fourcc[4] = '\0';
    }

    printf("\r\n========== AVI INFO ==========\r\n");
    printf("File       : %s\r\n", filename);
    printf("Video      : %lu x %lu\r\n",
           (unsigned long)g_avi_app.info.width,
           (unsigned long)g_avi_app.info.height);
    printf("FourCC     : %.4s\r\n", g_avi_app.info.video_fourcc);
    printf("Frame rate : %lu.%03lu FPS\r\n",
           (unsigned long)(g_avi_app.info.fps_x1000 / 1000U),
           (unsigned long)(g_avi_app.info.fps_x1000 % 1000U));
    printf("Frame count: %lu\r\n", (unsigned long)g_avi_app.info.frame_count);
    printf("Max frame  : %lu bytes\r\n",
           (unsigned long)g_avi_app.info.max_frame_size);
    printf("Audio tracks: %lu (playback disabled)\r\n",
           (unsigned long)g_avi_app.info.audio_track_count);

    for(i = 0; i < audio_count; i++) {
        if(AVI_set_audio_track(avi, i) != 0) continue;
        if(i == 0) {
            g_avi_app.info.audio_format = (uint32_t)AVI_audio_format(avi);
            g_avi_app.info.audio_channels = (uint32_t)AVI_audio_channels(avi);
            g_avi_app.info.audio_sample_rate = (uint32_t)AVI_audio_rate(avi);
            g_avi_app.info.audio_bits = (uint32_t)AVI_audio_bits(avi);
        }
        printf("Audio[%d]   : format=%d, channels=%d, rate=%ld Hz, bits=%d, chunks=%ld\r\n",
               i, AVI_audio_format(avi), AVI_audio_channels(avi),
               AVI_audio_rate(avi), AVI_audio_bits(avi), AVI_audio_chunks(avi));
    }
    if(audio_count > 0) AVI_set_audio_track(avi, 0);
    printf("==============================\r\n\r\n");
}

AVI_APP_RESULT AVI_APP_Init(const char *filename)
{
    avi_t *avi;

    if(filename == NULL || filename[0] == '\0')
        return AVI_APP_ERR_PARAM;
    if(g_avi_app.state != AVI_APP_STATE_CLOSED)
        return AVI_APP_ERR_STATE;

    memset(&g_avi_app, 0, sizeof(g_avi_app));
    g_avi_app.state = AVI_APP_STATE_CLOSED;
    g_avi_app.speed = AVI_APP_SPEED_1_00X;
    g_avi_app.jpeg_scale = AVI_APP_JPEG_SCALE_1_1;
    g_avi_app.loop_mode = AVI_APP_LOOP_DISABLE;

    avi = AVI_open_input_file(filename, 1);
    if(avi == NULL) {
        long err = AVI_get_error();
        printf("[AVI] open/parse failed, error=%ld\r\n", err);
        return (err == AVI_ERR_OPEN) ? AVI_APP_ERR_OPEN : AVI_APP_ERR_FORMAT;
    }
    g_avi_app.avi = avi;

    if(!avi->index_ready || avi->video_frames <= 0) {
        AVI_close(avi);
        memset(&g_avi_app, 0, sizeof(g_avi_app));
        return AVI_APP_ERR_NO_INDEX;
    }
    if(!avi_app_is_mjpeg(avi)) {
        const char *fcc = avi_app_video_fourcc(avi);
        printf("[AVI] unsupported video codec: %.4s (TJpgDec needs MJPEG/JPEG)\r\n",
               fcc ? fcc : "????");
        AVI_close(avi);
        memset(&g_avi_app, 0, sizeof(g_avi_app));
        return AVI_APP_ERR_FORMAT;
    }
    if(AVI_video_width(avi) <= 0 || AVI_video_height(avi) <= 0 ||
       AVI_frame_rate(avi) <= 0.0) {
        AVI_close(avi);
        memset(&g_avi_app, 0, sizeof(g_avi_app));
        return AVI_APP_ERR_FORMAT;
    }

    avi_app_print_info(filename);

    g_avi_app.lock = xSemaphoreCreateMutex();
    g_avi_app.exit_sem = xSemaphoreCreateBinary();
    if(g_avi_app.lock == NULL || g_avi_app.exit_sem == NULL) {
        if(g_avi_app.lock != NULL) vSemaphoreDelete(g_avi_app.lock);
        if(g_avi_app.exit_sem != NULL) vSemaphoreDelete(g_avi_app.exit_sem);
        AVI_close(avi);
        memset(&g_avi_app, 0, sizeof(g_avi_app));
        return AVI_APP_ERR_RTOS;
    }

    g_avi_app.timer = xTimerCreate("avi_frame", avi_app_timer_period(), pdTRUE,
                                   NULL, avi_app_timer_callback);
    if(g_avi_app.timer == NULL) {
        vSemaphoreDelete(g_avi_app.lock);
        vSemaphoreDelete(g_avi_app.exit_sem);
        AVI_close(avi);
        memset(&g_avi_app, 0, sizeof(g_avi_app));
        return AVI_APP_ERR_RTOS;
    }

    if(xTaskCreate(avi_app_worker, "avi_play", AVI_APP_TASK_STACK_SIZE,
                   NULL, AVI_APP_TASK_PRIORITY, &g_avi_app.task) != pdPASS) {
        xTimerDelete(g_avi_app.timer, portMAX_DELAY);
        vSemaphoreDelete(g_avi_app.lock);
        vSemaphoreDelete(g_avi_app.exit_sem);
        AVI_close(avi);
        memset(&g_avi_app, 0, sizeof(g_avi_app));
        return AVI_APP_ERR_RTOS;
    }

    g_avi_app.state = AVI_APP_STATE_READY;
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_DeInit(void)
{
    TimerHandle_t timer;
    SemaphoreHandle_t exit_sem;

    if(g_avi_app.state == AVI_APP_STATE_CLOSED)
        return AVI_APP_OK;

    timer = g_avi_app.timer;
    exit_sem = g_avi_app.exit_sem;
    if(timer != NULL)
        xTimerStop(timer, portMAX_DELAY);

    g_avi_app.state = AVI_APP_STATE_STOPPED;
    g_avi_app.exit_requested = 1;
    if(g_avi_app.task != NULL)
        xTaskNotify(g_avi_app.task, AVI_APP_NOTIFY_EXIT, eSetBits);

    if(exit_sem != NULL)
        xSemaphoreTake(exit_sem, portMAX_DELAY);

    if(timer != NULL)
        xTimerDelete(timer, portMAX_DELAY);
    if(g_avi_app.lock != NULL)
        vSemaphoreDelete(g_avi_app.lock);
    if(exit_sem != NULL)
        vSemaphoreDelete(exit_sem);
    if(g_avi_app.avi != NULL)
        AVI_close(g_avi_app.avi);

    memset(&g_avi_app, 0, sizeof(g_avi_app));
    g_avi_app.state = AVI_APP_STATE_CLOSED;
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_Play(AVI_APP_SPEED speed)
{
    if(!avi_app_speed_valid(speed)) return AVI_APP_ERR_PARAM;
    if(g_avi_app.avi == NULL || g_avi_app.timer == NULL)
        return AVI_APP_ERR_STATE;

    if(g_avi_app.current_frame >= (uint32_t)g_avi_app.avi->video_frames) {
        if(xSemaphoreTake(g_avi_app.lock, portMAX_DELAY) != pdTRUE)
            return AVI_APP_ERR_RTOS;
        g_avi_app.current_frame = 0;
        AVI_set_video_position(g_avi_app.avi, 0);
        xSemaphoreGive(g_avi_app.lock);
    }

    g_avi_app.speed = speed;
    g_avi_app.state = AVI_APP_STATE_PLAYING;
    if(xTimerChangePeriod(g_avi_app.timer, avi_app_timer_period(), portMAX_DELAY) != pdPASS)
        return AVI_APP_ERR_RTOS;
    xTaskNotify(g_avi_app.task, AVI_APP_NOTIFY_TICK, eSetBits);
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_Resume(void)
{
    if(g_avi_app.state != AVI_APP_STATE_PAUSED)
        return AVI_APP_ERR_STATE;
    return AVI_APP_Play(g_avi_app.speed);
}

AVI_APP_RESULT AVI_APP_Pause(void)
{
    if(g_avi_app.state != AVI_APP_STATE_PLAYING)
        return AVI_APP_ERR_STATE;
    g_avi_app.state = AVI_APP_STATE_PAUSED;
    if(xTimerStop(g_avi_app.timer, portMAX_DELAY) != pdPASS)
        return AVI_APP_ERR_RTOS;
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_Stop(void)
{
    if(g_avi_app.avi == NULL || g_avi_app.timer == NULL)
        return AVI_APP_ERR_STATE;
    g_avi_app.state = AVI_APP_STATE_STOPPED;
    if(xTimerStop(g_avi_app.timer, portMAX_DELAY) != pdPASS)
        return AVI_APP_ERR_RTOS;
    if(xSemaphoreTake(g_avi_app.lock, portMAX_DELAY) != pdTRUE)
        return AVI_APP_ERR_RTOS;
    g_avi_app.current_frame = 0;
    AVI_set_video_position(g_avi_app.avi, 0);
    xSemaphoreGive(g_avi_app.lock);
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_SetSpeed(AVI_APP_SPEED speed)
{
    if(!avi_app_speed_valid(speed)) return AVI_APP_ERR_PARAM;
    if(g_avi_app.avi == NULL) return AVI_APP_ERR_STATE;
    g_avi_app.speed = speed;
    if(g_avi_app.state == AVI_APP_STATE_PLAYING &&
       xTimerChangePeriod(g_avi_app.timer, avi_app_timer_period(), portMAX_DELAY) != pdPASS)
        return AVI_APP_ERR_RTOS;
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_FastForward(AVI_APP_SPEED speed)
{
    if(!avi_app_speed_valid(speed) || speed <= AVI_APP_SPEED_1_00X)
        return AVI_APP_ERR_PARAM;
    if(g_avi_app.state == AVI_APP_STATE_PAUSED)
        return AVI_APP_Play(speed);
    if(g_avi_app.state != AVI_APP_STATE_PLAYING)
        return AVI_APP_Play(speed);
    return AVI_APP_SetSpeed(speed);
}

AVI_APP_RESULT AVI_APP_Seek(AVI_APP_SEEK_MODE mode, uint32_t value)
{
    uint32_t total, target;
    uint64_t target64;
    int was_playing;

    if(g_avi_app.avi == NULL || !g_avi_app.avi->index_ready)
        return AVI_APP_ERR_STATE;
    total = (uint32_t)g_avi_app.avi->video_frames;
    if(total == 0U) return AVI_APP_ERR_NO_INDEX;

    if(mode == AVI_APP_SEEK_ABSOLUTE_FRAME) {
        target64 = value;
    } else if(mode == AVI_APP_SEEK_FORWARD_FRAMES) {
        target64 = (uint64_t)g_avi_app.current_frame + value;
    } else if(mode == AVI_APP_SEEK_BACKWARD_FRAMES) {
        target64 = (value > g_avi_app.current_frame) ? 0U :
                   (uint64_t)g_avi_app.current_frame - value;
    } else if(mode == AVI_APP_SEEK_BEGIN) {
        target64 = 0U;
    } else if(mode == AVI_APP_SEEK_END) {
        target64 = total - 1U;
    } else {
        return AVI_APP_ERR_PARAM;
    }
    if(target64 >= total) target64 = total - 1U;
    target = (uint32_t)target64;

    was_playing = (g_avi_app.state == AVI_APP_STATE_PLAYING);
    if(xSemaphoreTake(g_avi_app.lock, portMAX_DELAY) != pdTRUE)
        return AVI_APP_ERR_RTOS;
    g_avi_app.current_frame = target;
    AVI_set_video_position(g_avi_app.avi, (long)target);
    xSemaphoreGive(g_avi_app.lock);

    if(was_playing)
        xTaskNotify(g_avi_app.task, AVI_APP_NOTIFY_TICK, eSetBits);
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_SeekFrame(uint32_t frame)
{
    return AVI_APP_Seek(AVI_APP_SEEK_ABSOLUTE_FRAME, frame);
}

AVI_APP_RESULT AVI_APP_SetJpegScale(AVI_APP_JPEG_SCALE scale)
{
    if((unsigned)scale > (unsigned)AVI_APP_JPEG_SCALE_1_8)
        return AVI_APP_ERR_PARAM;
    g_avi_app.jpeg_scale = scale;
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_SetLoop(AVI_APP_LOOP_MODE mode)
{
    if(mode != AVI_APP_LOOP_DISABLE && mode != AVI_APP_LOOP_ENABLE)
        return AVI_APP_ERR_PARAM;
    g_avi_app.loop_mode = mode;
    return AVI_APP_OK;
}

AVI_APP_RESULT AVI_APP_SetDisplayPosition(uint16_t x, uint16_t y)
{
    g_avi_app.display_x = x;
    g_avi_app.display_y = y;
    return AVI_APP_OK;
}

AVI_APP_STATE AVI_APP_GetState(void)
{
    return g_avi_app.state;
}

uint32_t AVI_APP_GetCurrentFrame(void)
{
    return g_avi_app.current_frame;
}

AVI_APP_RESULT AVI_APP_GetInfo(AVI_APP_INFO *info)
{
    if(info == NULL) return AVI_APP_ERR_PARAM;
    if(g_avi_app.avi == NULL) return AVI_APP_ERR_STATE;
    *info = g_avi_app.info;
    return AVI_APP_OK;
}

avi_t *AVI_APP_GetAVIHandle(void)
{
    return g_avi_app.avi;
}

AVI_APP_RESULT AVI_APP_SetAudioHook(AVI_APP_AUDIO_HOOK hook, void *user)
{
    g_avi_app.audio_hook = hook;
    g_avi_app.audio_user = user;
    return AVI_APP_OK;
}
