#include "audio_wave.h"
#include "wm8978.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define AUDIO_WAVE_READ_SIZE       4096U
#define AUDIO_WAVE_DMA_BUFFERS     2U

/* All file handles and large working buffers are placed in D1 SRAM. */
__attribute__((section(".RAM_D1"), aligned(4))) static FIL      wave_file;
__attribute__((section(".RAM_D1"), aligned(32))) static uint8_t wave_read_buf[AUDIO_WAVE_READ_SIZE];

static AUDIO_WAVE_Info  wave_info;
static volatile uint8_t wave_ready;
static volatile uint8_t wave_playing;
static volatile uint8_t wave_eof;
static volatile uint8_t wave_error;
static volatile uint8_t wave_free_mask;
static uint8_t          wave_buffer_started[2];
static uint32_t         wave_file_pos;

static uint16_t wave_u16(const uint8_t *p)
{
    return (uint16_t)p[0]|((uint16_t)p[1]<<8);
}

static uint32_t wave_u32(const uint8_t *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

static AUDIO_WAVE_Status wave_read_exact(void *dst, UINT size)
{
    UINT br = 0;
    FRESULT fr = f_read(&wave_file, dst, size, &br);
    if (fr != FR_OK || br != size) return AUDIO_WAVE_ERROR;
    return AUDIO_WAVE_OK;
}

static AUDIO_WAVE_Status wave_parse(void)
{
    uint8_t head[12];
    uint8_t chunk[8];
    uint8_t fmt[40];
    uint32_t riff_size;
    uint8_t have_fmt = 0;
    uint8_t have_data = 0;

    if (wave_read_exact(head,sizeof(head))!=AUDIO_WAVE_OK)  return AUDIO_WAVE_ERROR;

    if (memcmp(head,"RIFF",4) || memcmp(head + 8,"WAVE",4)) return AUDIO_WAVE_UNSUPPORTED;

    riff_size = wave_u32(head + 4);
    (void)riff_size;

    while (f_tell(&wave_file) + 8U <= f_size(&wave_file))
    {
        uint32_t chunk_size;
        uint32_t chunk_data_pos;

        if (wave_read_exact(chunk, sizeof(chunk)) != AUDIO_WAVE_OK) return AUDIO_WAVE_ERROR;

        chunk_size = wave_u32(chunk + 4);
        chunk_data_pos = f_tell(&wave_file);

        if (!memcmp(chunk, "fmt ", 4))
        {
            uint32_t read_size = chunk_size;
            if (read_size < 16U)         return AUDIO_WAVE_UNSUPPORTED;
            if (read_size > sizeof(fmt)) read_size = sizeof(fmt);

            memset(fmt, 0, sizeof(fmt));
            if (wave_read_exact(fmt, read_size) != AUDIO_WAVE_OK) return AUDIO_WAVE_ERROR;

            wave_info.format_tag = wave_u16(fmt);
            wave_info.channels = wave_u16(fmt + 2);
            wave_info.sample_rate = wave_u32(fmt + 4);
            wave_info.byte_rate = wave_u32(fmt + 8);
            wave_info.block_align = wave_u16(fmt + 12);
            wave_info.bits_per_sample = wave_u16(fmt + 14);

            have_fmt = 1;
        }
        else if (!memcmp(chunk, "data", 4))
        {
            wave_info.data_offset = chunk_data_pos;
            wave_info.data_size = chunk_size;
            have_data = 1;
        }

        if (have_fmt && have_data) break;

        if (f_lseek(&wave_file, chunk_data_pos + chunk_size + (chunk_size & 1U)) != FR_OK) return AUDIO_WAVE_ERROR;
    }

    if (!have_fmt || !have_data)    return AUDIO_WAVE_ERROR;

    if (wave_info.format_tag != 1U) return AUDIO_WAVE_UNSUPPORTED;

    if ((wave_info.channels != 1U && wave_info.channels != 2U) ||
        (wave_info.bits_per_sample != 8U &&
         wave_info.bits_per_sample != 16U &&
         wave_info.bits_per_sample != 24U &&
         wave_info.bits_per_sample != 32U))
        return AUDIO_WAVE_UNSUPPORTED;

    if (wave_info.block_align !=
        (uint16_t)(wave_info.channels * (wave_info.bits_per_sample / 8U)))
        return AUDIO_WAVE_UNSUPPORTED;

    if (wave_info.sample_rate != 8000U && wave_info.sample_rate != 11025U &&
        wave_info.sample_rate != 12000U && wave_info.sample_rate != 16000U &&
        wave_info.sample_rate != 22050U && wave_info.sample_rate != 24000U &&
        wave_info.sample_rate != 32000U && wave_info.sample_rate != 44100U &&
        wave_info.sample_rate != 48000U)
        return AUDIO_WAVE_UNSUPPORTED;

    wave_info.data_remaining = wave_info.data_size;
    wave_file_pos = wave_info.data_offset;

    if (f_lseek(&wave_file, wave_info.data_offset) != FR_OK) return AUDIO_WAVE_ERROR;

    printf("========== WAV INFO ==========\r\n");
    printf("Sample rate : %lu Hz\r\n", (unsigned long)wave_info.sample_rate);
    printf("Channels    : %u\r\n", wave_info.channels);
    printf("Bits/sample : %u\r\n", wave_info.bits_per_sample);
    printf("Format      : PCM\r\n");
    printf("Block align : %u bytes\r\n", wave_info.block_align);
    printf("Byte rate   : %lu bytes/s\r\n", (unsigned long)wave_info.byte_rate);
    printf("Data size   : %lu bytes\r\n", (unsigned long)wave_info.data_size);
    printf("Duration    : %.2f s\r\n",
           wave_info.byte_rate ? (double)wave_info.data_size / wave_info.byte_rate : 0.0);
    printf("==============================\r\n");

    return AUDIO_WAVE_OK;
}

static int16_t wave_decode_sample(const uint8_t *p, uint16_t bits)
{
    int32_t value;

    switch (bits)
    {
        case 8:
            return (int16_t)(((int16_t)p[0] - 128) << 8);

        case 16:
            return (int16_t)wave_u16(p);

        case 24:
            value = (int32_t)((uint32_t)p[0] |
                    ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16));
            if (value & 0x800000)
                value |= (int32_t)0xFF000000;
            return (int16_t)(value >> 8);

        case 32:
            value = (int32_t)wave_u32(p);
            return (int16_t)(value >> 16);

        default:
            return 0;
    }
}

/* Fill one WM8978 stereo buffer. This function runs in task context, never in ISR. */
static AUDIO_WAVE_Status wave_fill_buffer(uint8_t index)
{
    int16_t *out;
    uint32_t out_frames;
    uint32_t produced = 0;
    uint32_t bytes_per_sample;
    uint32_t frame_bytes;

    out = wm8978_audio_get_buffer(index);
    out_frames = wm8978_audio_get_buffer_frames();

    if (!out || !out_frames) return AUDIO_WAVE_ERROR;

    bytes_per_sample = wave_info.bits_per_sample / 8U;
    frame_bytes = wave_info.block_align;

    while (produced < out_frames && wave_info.data_remaining >= frame_bytes)
    {
        UINT request = AUDIO_WAVE_READ_SIZE;
        UINT br = 0;
        uint32_t frames;
        uint32_t i;

        request -= request % frame_bytes;
        if (request > wave_info.data_remaining)
            request = (UINT)(wave_info.data_remaining - (wave_info.data_remaining % frame_bytes));

        if (request == 0U) break;

        if (f_read(&wave_file, wave_read_buf, request, &br) != FR_OK || br == 0U) return AUDIO_WAVE_ERROR;

        br -= br % frame_bytes;
        if (br == 0U) return AUDIO_WAVE_ERROR;

        wave_info.data_remaining -= br;
        wave_file_pos += br;
        frames = br / frame_bytes;

        for (i = 0; i < frames && produced < out_frames; i++)
        {
            const uint8_t *p = &wave_read_buf[i * frame_bytes];
            int16_t left = wave_decode_sample(p, wave_info.bits_per_sample);
            int16_t right;

            if (wave_info.channels == 2U) right = wave_decode_sample(p + bytes_per_sample, wave_info.bits_per_sample);
            else                          right = left;

            out[produced * 2U] = left;
            out[produced * 2U + 1U] = right;
            produced++;
        }

        if (frames > produced && produced == out_frames) break;
    }

    if (produced < out_frames)
    {
        memset(&out[produced * 2U], 0, (out_frames - produced) * 2U * sizeof(int16_t));
        wave_eof = 1U;
    }

    if (wm8978_audio_submit_buffer(index) != HAL_OK) return AUDIO_WAVE_ERROR;

    wave_buffer_started[index] = 1U;
    return AUDIO_WAVE_OK;
}

/* Called by WM8978 DMA ISR. Only marks a buffer free; no FatFs calls here. */
static void wave_dma_callback(uint8_t index, int16_t *buffer, uint32_t halfwords)
{
    (void)buffer;
    (void)halfwords;

    if (index < 2U) wave_free_mask |= (uint8_t)(1U << index);
}

AUDIO_WAVE_Status audio_wave_init(const char *path)
{
    FRESULT fr;

    if (!path) return AUDIO_WAVE_ERROR;

    if (wave_playing) audio_wave_stop();

    wave_ready = 0U;
    wave_playing = 0U;
    wave_eof = 0U;
    wave_error = 0U;
    wave_free_mask = 0U;
    memset(&wave_info, 0, sizeof(wave_info));
    memset(wave_buffer_started, 0, sizeof(wave_buffer_started));

    fr = f_open(&wave_file, path, FA_READ);
    if (fr != FR_OK)
    {
        printf("WAV open failed: %d, %s\r\n", (int)fr, path);
        return AUDIO_WAVE_ERROR;
    }

    if (wave_parse() != AUDIO_WAVE_OK)
    {
        printf("WAV parse/format error: %s\r\n", path);
        f_close(&wave_file);
        return AUDIO_WAVE_UNSUPPORTED;
    }

    wave_ready = 1U;
    return AUDIO_WAVE_OK;
}

AUDIO_WAVE_Status audio_wave_start(void)
{
    if (!wave_ready) return AUDIO_WAVE_NOT_READY;

    if (wm8978_set_sample_rate(wave_info.sample_rate) != HAL_OK)
    {
        /* If codec was initialized at this rate already, this is not expected to fail. */
        return AUDIO_WAVE_ERROR;
    }

    wave_info.data_remaining = wave_info.data_size;
    wave_file_pos = wave_info.data_offset;
    wave_eof = 0U;
    wave_error = 0U;
    wave_free_mask = 0U;
    memset(wave_buffer_started, 0, sizeof(wave_buffer_started));

    if (f_lseek(&wave_file, wave_info.data_offset) != FR_OK) return AUDIO_WAVE_ERROR;

    if (wave_fill_buffer(0U) != AUDIO_WAVE_OK) return AUDIO_WAVE_ERROR;
    if (wave_fill_buffer(1U) != AUDIO_WAVE_OK) return AUDIO_WAVE_ERROR;

    wm8978_audio_set_callback(wave_dma_callback);

    if (wm8978_audio_start() != HAL_OK) return AUDIO_WAVE_ERROR;

    wave_playing = 1U;
    return AUDIO_WAVE_OK;
}

AUDIO_WAVE_Status audio_wave_process(void)
{
    uint8_t mask;
    uint8_t i;

    if (!wave_playing) return wave_error ? AUDIO_WAVE_ERROR : AUDIO_WAVE_NOT_READY;

    mask = wave_free_mask;
    wave_free_mask = 0U;

    for (i = 0; i < AUDIO_WAVE_DMA_BUFFERS; i++)
    {
        if (mask & (1U << i))
        {
            if (wave_info.data_remaining > 0U)
            {
                if (wave_fill_buffer(i) != AUDIO_WAVE_OK)
                {
                    wave_error = 1U;
                    audio_wave_stop();
                    return AUDIO_WAVE_ERROR;
                }
            }
            else
            {
                /* Leave zero-filled buffer in place for one final DMA cycle. */
                wave_eof = 1U;
            }
        }
    }

    if (wave_eof && wave_info.data_remaining == 0U)
    {
        /* The last partially filled buffer is padded with silence.
           The application may call audio_wave_stop() after observing EOF. */
    }

    if (wave_eof && wave_info.data_remaining == 0U) return AUDIO_WAVE_EOF;

    return AUDIO_WAVE_OK;
}

void audio_wave_stop(void)
{
    if (wave_playing) wm8978_audio_stop();

    wave_playing = 0U;
    wave_ready = 0U;
    wm8978_audio_set_callback(NULL);

    if (wave_file.obj.fs != NULL) f_close(&wave_file);
}

uint8_t audio_wave_is_playing(void)
{
    return wave_playing;
}

uint32_t audio_wave_get_sample_rate(void) { return wave_info.sample_rate; }
uint16_t audio_wave_get_channels(void) { return wave_info.channels; }
uint16_t audio_wave_get_bits_per_sample(void) { return wave_info.bits_per_sample; }
uint32_t audio_wave_get_data_size(void) { return wave_info.data_size; }
uint32_t audio_wave_get_data_remaining(void) { return wave_info.data_remaining; }
