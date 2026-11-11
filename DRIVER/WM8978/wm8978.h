#ifndef _WM8978_H_
#define _WM8978_H_

#include "stm32h7xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WM8978_I2C_ADDR_7BIT              0x1AU
#define WM8978_I2C_ADDR_8BIT              (WM8978_I2C_ADDR_7BIT << 1)

#define WM8978_TX_BUFFER_COUNT             2U
#define WM8978_TX_BUFFER_HALFWORDS         (10U * 1024U)
#define WM8978_TX_BUFFER_STEREO_FRAMES     (WM8978_TX_BUFFER_HALFWORDS / 2U)

#define WM8978_OK                          0U
#define WM8978_ERROR                       1U

typedef enum
{
    WM8978_OUTPUT_SPEAKER = 0,      /* LOUT2/ROUT2, BTL-style speaker connection */
    WM8978_OUTPUT_HEADPHONE,       /* LOUT1/ROUT1 */
    WM8978_OUTPUT_BOTH             /* LOUT1/ROUT1 + LOUT2/ROUT2 */
} WM8978_OutputType;

typedef void (*WM8978_BufferCallback)(uint8_t index,
                                      int16_t *buffer,
                                      uint32_t halfwords);

/* Codec and STM32H743 I2S/DMA initialization. sample_rate: 8000..48000 supported values. */
HAL_StatusTypeDef wm8978_init(uint32_t sample_rate, WM8978_OutputType output);
void wm8978_deinit(void);

/* WM8978 register access. The chip has no normal register readback; read_reg returns cache. */
HAL_StatusTypeDef wm8978_write_reg(uint8_t reg, uint16_t value);
uint16_t wm8978_read_reg(uint8_t reg);
HAL_StatusTypeDef wm8978_reset(void);

/* Codec controls */
HAL_StatusTypeDef wm8978_set_sample_rate(uint32_t sample_rate);
HAL_StatusTypeDef wm8978_set_volume(uint8_t volume);       /* 0..63; 57 = 0dB */
HAL_StatusTypeDef wm8978_set_volume_db(int8_t db);         /* -57..+6dB */
HAL_StatusTypeDef wm8978_set_dac_volume(uint8_t left, uint8_t right); /* 0..255; 255 = 0dB */
HAL_StatusTypeDef wm8978_set_mute(uint8_t mute);            /* 1=mute, 0=unmute */
HAL_StatusTypeDef wm8978_set_output(WM8978_OutputType output);

/* PCM buffer API. PCM layout: L0,R0,L1,R1... signed 16-bit. */
int16_t *wm8978_audio_get_buffer(uint8_t index);
uint32_t wm8978_audio_get_buffer_halfwords(void);
uint32_t wm8978_audio_get_buffer_frames(void);
uint8_t wm8978_audio_get_free_buffer_index(void);
HAL_StatusTypeDef wm8978_audio_submit_buffer(uint8_t index);
void wm8978_audio_set_callback(WM8978_BufferCallback callback);

/* DMA playback control */
HAL_StatusTypeDef wm8978_audio_start(void);
void wm8978_audio_stop(void);
uint8_t wm8978_audio_is_running(void);
uint32_t wm8978_audio_get_dma_error(void);

#ifdef __cplusplus
}
#endif

#endif
