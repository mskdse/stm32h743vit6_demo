#ifndef AUDIO_WAVE_H
#define AUDIO_WAVE_H

#include "stm32h7xx_hal.h"
#include "ff.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    AUDIO_WAVE_OK = 0,
    AUDIO_WAVE_ERROR,
    AUDIO_WAVE_NOT_READY,
    AUDIO_WAVE_UNSUPPORTED,
    AUDIO_WAVE_EOF
} AUDIO_WAVE_Status;

typedef struct
{
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t data_remaining;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint16_t block_align;
    uint16_t format_tag;
} AUDIO_WAVE_Info;

/* Parse WAV header and print file information. FatFs must already be mounted. */
AUDIO_WAVE_Status audio_wave_init(const char *path);

/* Start playback after audio_wave_init(). */
AUDIO_WAVE_Status audio_wave_start(void);

/* Call frequently from an audio task. It refills DMA buffers released by DMA. */
AUDIO_WAVE_Status audio_wave_process(void);

/* Stop playback and close the WAV file. */
void audio_wave_stop(void);

/* Current playback state. */
uint8_t audio_wave_is_playing(void);

/* WAV information getters. */
uint32_t audio_wave_get_sample_rate(void);
uint16_t audio_wave_get_channels(void);
uint16_t audio_wave_get_bits_per_sample(void);
uint32_t audio_wave_get_data_size(void);
uint32_t audio_wave_get_data_remaining(void);

#ifdef __cplusplus
}
#endif

#endif
