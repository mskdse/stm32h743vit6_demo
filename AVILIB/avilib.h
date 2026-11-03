#ifndef AVILIB_H
#define AVILIB_H

#include <stdint.h>
#include <stdio.h>
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * STM32H743 lightweight AVI 1.0 library.
 * The large indexes are stored in avi_t; the single global instance in
 * avilib.c is placed in .RAM_D1. No malloc/free is used.
 */
#define MAX_AVI_FRAMES          8000U
#define MAX_AVI_AUDIO_CHUNKS    8000U
#define AVI_MAX_TRACKS          2U
#define HEADERBYTES             (5*1024U)
#define AVI_STATIC_LIMIT_BYTES  (450U*1024U)

#define AVI_MODE_WRITE  0
#define AVI_MODE_READ   1

/* Error codes */
#define AVI_ERR_NONE            0
#define AVI_ERR_SIZELIM         1
#define AVI_ERR_OPEN            2
#define AVI_ERR_READ            3
#define AVI_ERR_WRITE           4
#define AVI_ERR_WRITE_INDEX     5
#define AVI_ERR_CLOSE           6
#define AVI_ERR_NOT_PERM        7
#define AVI_ERR_NO_MEM          8
#define AVI_ERR_NO_AVI          9
#define AVI_ERR_NO_HDRL        10
#define AVI_ERR_NO_MOVI        11
#define AVI_ERR_NO_VIDS        12
#define AVI_ERR_NO_IDX         13
#define AVI_ERR_INDEX_LIMIT    14
#define AVI_ERR_UNSUPPORTED    15

/* All file positions are 32-bit; files larger than 4 GiB are rejected. */
typedef uint32_t avi_off_t;

typedef struct {
    avi_off_t pos;       /* Absolute file position of chunk data */
    uint32_t len;
    uint32_t key;        /* AVIIF_KEYFRAME (0x10) when known */
} video_index_entry;

typedef struct {
    avi_off_t pos;       /* Absolute file position of chunk data */
    uint32_t len;
    uint32_t tot;        /* Total bytes preceding this chunk in this track */
} audio_index_entry;

typedef struct track_s {
    long a_fmt;
    long a_chans;
    long a_rate;
    long a_bits;
    long mp3rate;                 /* Approximate kbit/s, retained for compatibility */
    long audio_strn;              /* Actual AVI stream number */
    uint32_t audio_bytes;
    long audio_chunks;
    char audio_tag[4];
    long audio_posc;
    long audio_posb;
    avi_off_t a_codech_off;
    avi_off_t a_codecf_off;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t cb_size;
    uint32_t stream_scale;
    uint32_t stream_rate;
    uint32_t stream_length;
    uint32_t max_chunk;
    audio_index_entry audio_index[MAX_AVI_AUDIO_CHUNKS];
} track_t;

typedef struct {
    uint32_t bi_size;
    int32_t  bi_width;
    int32_t  bi_height;
    uint16_t bi_planes;
    uint16_t bi_bit_count;
    uint32_t bi_compression;
    uint32_t bi_size_image;
    int32_t  bi_x_pels_per_meter;
    int32_t  bi_y_pels_per_meter;
    uint32_t bi_clr_used;
    uint32_t bi_clr_important;
} BITMAPINFOHEADER_avilib;

typedef struct {
    uint16_t w_format_tag;
    uint16_t n_channels;
    uint32_t n_samples_per_sec;
    uint32_t n_avg_bytes_per_sec;
    uint16_t n_block_align;
    uint16_t w_bits_per_sample;
    uint16_t cb_size;
} WAVEFORMATEX_avilib;

typedef struct {
    FIL fdes;
    long mode;
    long width;
    long height;
    double fps;
    char compressor[8];
    char compressor2[8];
    long video_strn;
    long video_frames;
    char video_tag[4];
    long video_pos;

    uint32_t max_len;
    track_t track[AVI_MAX_TRACKS];

    avi_off_t pos;               /* Current write position */
    long n_idx;                  /* Number of indexed chunks */
    long max_idx;                /* Number of idx1 entries or generated entries */

    avi_off_t v_codech_off;
    avi_off_t v_codecf_off;

    video_index_entry video_index[MAX_AVI_FRAMES];

    avi_off_t last_pos;
    uint32_t last_len;
    int must_use_index;
    avi_off_t movi_start;        /* First chunk header in movi */
    avi_off_t movi_end;          /* Exclusive end of movi payload */
    avi_off_t movi_type_pos;     /* File position of the fourcc "movi" */
    avi_off_t riff_end;
    avi_off_t file_size;
    avi_off_t idx1_pos;
    uint32_t idx1_len;
    uint32_t header_video_frames;
    uint32_t video_stream_scale;
    uint32_t video_stream_rate;
    uint32_t video_stream_length;
    uint32_t video_stream_sample_size;
    uint32_t video_stream_suggested_size;
    uint32_t audio_stream_count;
    uint32_t video_chunk_count;
    uint32_t audio_chunk_count;

    int anum;
    int aptr;
    int has_idx1;
    int index_ready;
    int is_open;
    int video_is_compressed;

    BITMAPINFOHEADER_avilib bitmap_info_header;
    WAVEFORMATEX_avilib wave_format_ex[AVI_MAX_TRACKS];
} avi_t;

extern long AVI_errno;

/* Open / close */
avi_t *AVI_open_input_file(const char *filename, int getIndex);
avi_t *AVI_open_output_file(char *filename);
int AVI_close(avi_t *AVI);

/* Write mode */
void AVI_set_video(avi_t *AVI, int width, int height, double fps, char *compressor);
void AVI_set_audio(avi_t *AVI, int channels, long rate, int bits, int format, long mp3rate);
int AVI_write_frame(avi_t *AVI, char *data, long bytes, int keyframe);
int AVI_write_audio(avi_t *AVI, char *data, long bytes);
int AVI_append_audio(avi_t *AVI, char *data, long bytes);
int AVI_dup_frame(avi_t *AVI);

/* Video information */
long AVI_video_frames(avi_t *AVI);
int AVI_video_width(avi_t *AVI);
int AVI_video_height(avi_t *AVI);
double AVI_frame_rate(avi_t *AVI);
char *AVI_video_compressor(avi_t *AVI);
long AVI_max_video_chunk(avi_t *AVI);

/* Audio information */
int AVI_audio_tracks(avi_t *AVI);
int AVI_audio_channels(avi_t *AVI);
int AVI_audio_bits(avi_t *AVI);
int AVI_audio_format(avi_t *AVI);
long AVI_audio_rate(avi_t *AVI);
long AVI_audio_bytes(avi_t *AVI);
long AVI_audio_chunks(avi_t *AVI);
long AVI_audio_mp3rate(avi_t *AVI);
int AVI_set_audio_track(avi_t *AVI, int track);
int AVI_get_audio_track(avi_t *AVI);

/* Read / seek */
long AVI_frame_size(avi_t *AVI, long frame);
int AVI_seek_start(avi_t *AVI);
int AVI_set_video_position(avi_t *AVI, long frame);
long AVI_read_frame(avi_t *AVI, char *vidbuf, int *keyframe);
long AVI_read_frame_ex(avi_t *AVI, char *vidbuf, uint32_t capacity, int *keyframe);
int AVI_set_audio_position(avi_t *AVI, long byte);
long AVI_read_audio(avi_t *AVI, char *audbuf, long bytes);
long AVI_read_audio_chunk(avi_t *AVI, char *audbuf);

/* Miscellaneous */
uint64_t AVI_max_size(void);
long AVI_bytes_remain(avi_t *AVI);
long AVI_bytes_written(avi_t *AVI);
long AVI_get_error(void);

#ifdef __cplusplus
}
#endif

#endif /* AVILIB_H */
