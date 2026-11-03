#ifndef AVILIB_APP_H
#define AVILIB_APP_H

#include <stdint.h>
#include "avilib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Playback result codes */
typedef enum {
    AVI_APP_OK = 0,
    AVI_APP_ERR_PARAM,
    AVI_APP_ERR_STATE,
    AVI_APP_ERR_OPEN,
    AVI_APP_ERR_FORMAT,
    AVI_APP_ERR_NO_INDEX,
    AVI_APP_ERR_RTOS,
    AVI_APP_ERR_JPEG_PREPARE,
    AVI_APP_ERR_JPEG_DECODE,
    AVI_APP_ERR_FILE_READ
} AVI_APP_RESULT;

/* Playback state */
typedef enum {
    AVI_APP_STATE_CLOSED = 0,
    AVI_APP_STATE_READY,
    AVI_APP_STATE_PLAYING,
    AVI_APP_STATE_PAUSED,
    AVI_APP_STATE_STOPPED,
    AVI_APP_STATE_FINISHED,
    AVI_APP_STATE_ERROR
} AVI_APP_STATE;

/* Speed is expressed as a multiplier of the AVI frame rate. */
typedef enum {
    AVI_APP_SPEED_0_25X = 0,
    AVI_APP_SPEED_0_50X,
    AVI_APP_SPEED_0_75X,
    AVI_APP_SPEED_1_00X,
    AVI_APP_SPEED_1_25X,
    AVI_APP_SPEED_1_50X,
    AVI_APP_SPEED_2_00X,
    AVI_APP_SPEED_4_00X
} AVI_APP_SPEED;

/* TJpgDec scale: 0=1/1, 1=1/2, 2=1/4, 3=1/8. */
typedef enum {
    AVI_APP_JPEG_SCALE_1_1 = 0,
    AVI_APP_JPEG_SCALE_1_2,
    AVI_APP_JPEG_SCALE_1_4,
    AVI_APP_JPEG_SCALE_1_8
} AVI_APP_JPEG_SCALE;

typedef enum {
    AVI_APP_LOOP_DISABLE = 0,
    AVI_APP_LOOP_ENABLE
} AVI_APP_LOOP_MODE;

typedef enum {
    AVI_APP_SEEK_ABSOLUTE_FRAME = 0,
    AVI_APP_SEEK_FORWARD_FRAMES,
    AVI_APP_SEEK_BACKWARD_FRAMES,
    AVI_APP_SEEK_BEGIN,
    AVI_APP_SEEK_END
} AVI_APP_SEEK_MODE;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t max_frame_size;
    uint32_t fps_x1000;
    uint32_t audio_track_count;
    char video_fourcc[5];
    uint32_t audio_format;
    uint32_t audio_channels;
    uint32_t audio_sample_rate;
    uint32_t audio_bits;
} AVI_APP_INFO;

/*
 * Reserved audio hook. Audio is not played by this module yet.
 * The hook runs in the AVI playback task after a video frame is decoded.
 * It may use AVI_read_audio() to prepare its own audio DMA buffer. Do not
 * access the AVI FIL from another task while this hook/player is running.
 */
typedef void (*AVI_APP_AUDIO_HOOK)(avi_t *avi, uint32_t displayed_frame,
                                   uint32_t frame_period_ms, void *user);

/* 初始化、解析 AVI、打印信息 */
AVI_APP_RESULT AVI_APP_Init(const char *filename);

/* 关闭文件、释放资源 */
AVI_APP_RESULT AVI_APP_DeInit(void);

/* Start from current position (or frame 0 after STOP/FINISHED). */
AVI_APP_RESULT AVI_APP_Play(AVI_APP_SPEED speed);
AVI_APP_RESULT AVI_APP_Resume(void);
AVI_APP_RESULT AVI_APP_Pause(void);
AVI_APP_RESULT AVI_APP_Stop(void);

/* Playback controls. FastForward selects a speed greater than 1x. */
AVI_APP_RESULT AVI_APP_SetSpeed(AVI_APP_SPEED speed);
AVI_APP_RESULT AVI_APP_FastForward(AVI_APP_SPEED speed);
AVI_APP_RESULT AVI_APP_Seek(AVI_APP_SEEK_MODE mode, uint32_t value);
AVI_APP_RESULT AVI_APP_SeekFrame(uint32_t frame);

/* Display/playback parameters. */
AVI_APP_RESULT AVI_APP_SetJpegScale(AVI_APP_JPEG_SCALE scale);
AVI_APP_RESULT AVI_APP_SetLoop(AVI_APP_LOOP_MODE mode);
AVI_APP_RESULT AVI_APP_SetDisplayPosition(uint16_t x, uint16_t y);

/* Status and stream information. */
AVI_APP_STATE AVI_APP_GetState(void);
uint32_t AVI_APP_GetCurrentFrame(void);
AVI_APP_RESULT AVI_APP_GetInfo(AVI_APP_INFO *info);
avi_t *AVI_APP_GetAVIHandle(void);

/* Reserved for later audio output integration. NULL disables the hook. */
AVI_APP_RESULT AVI_APP_SetAudioHook(AVI_APP_AUDIO_HOOK hook, void *user);

#ifdef __cplusplus
}
#endif

#endif /* AVILIB_APP_H */
