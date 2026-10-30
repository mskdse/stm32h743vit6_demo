#ifndef AVILIB_H
#define AVILIB_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "ff.h"  // 绑定 FatFS

// 限制静态内存的规模，防止 H743 内存爆掉
#define MAX_AVI_FRAMES 5000        // 最大视频帧数
#define MAX_AVI_AUDIO_CHUNKS 5000  // 单轨道最大音频块数
#define AVI_MAX_TRACKS 2           // 最大音频轨道数 (为了省RAM，设为2)
#define HEADERBYTES 2048

typedef uint32_t off_t;
typedef int32_t  ssize_t;

typedef struct {
  off_t key;
  off_t pos;
  off_t len;
} video_index_entry;

typedef struct {
   off_t pos;
   off_t len;
   off_t tot;
} audio_index_entry;

typedef struct track_s {
    long   a_fmt;             
    long   a_chans;           
    long   a_rate;            
    long   a_bits;            
    long   mp3rate;           
    long   audio_strn;        
    off_t  audio_bytes;       
    long   audio_chunks;      
    char   audio_tag[4];      
    long   audio_posc;        
    long   audio_posb; 
    off_t  a_codech_off;        
    off_t  a_codecf_off;        
    // 静态音频索引数组
    audio_index_entry audio_index[MAX_AVI_AUDIO_CHUNKS]; 
} track_t;

typedef struct {
  uint32_t  bi_size;
  uint32_t  bi_width;
  uint32_t  bi_height;
  uint16_t  bi_planes;
  uint16_t  bi_bit_count;
  uint32_t  bi_compression;
  uint32_t  bi_size_image;
  uint32_t  bi_x_pels_per_meter;
  uint32_t  bi_y_pels_per_meter;
  uint32_t  bi_clr_used;
  uint32_t  bi_clr_important;
} BITMAPINFOHEADER_avilib;

typedef struct {
  uint16_t  w_format_tag;
  uint16_t  n_channels;
  uint32_t  n_samples_per_sec;
  uint32_t  n_avg_bytes_per_sec;
  uint16_t  n_block_align;
  uint16_t  w_bits_per_sample;
  uint16_t  cb_size;
} WAVEFORMATEX_avilib;

typedef struct {
  FIL    fdes;              // 直接使用 FatFS 句柄
  
  long   mode;              // 0 for writing, 1 for reading
  long   width;             
  long   height;            
  double fps;               
  char   compressor[8];     
  char   compressor2[8];    
  long   video_strn;        
  long   video_frames;      
  char   video_tag[4];      
  long   video_pos;         
  
  unsigned long max_len;    
  track_t track[AVI_MAX_TRACKS];  
  
  off_t  pos;        
  long   n_idx;             
  long   max_idx;           
  
  off_t  v_codech_off;       
  off_t  v_codecf_off;       
  
  // 静态视频索引数组
  unsigned char idx[MAX_AVI_FRAMES][16];               
  video_index_entry video_index[MAX_AVI_FRAMES];       
  
  off_t  last_pos;          
  unsigned long last_len;          
  int must_use_index;              
  off_t  movi_start;
  
  int anum;            
  int aptr;            
  
  // 静态头结构体，取消 malloc
  BITMAPINFOHEADER_avilib bitmap_info_header;
  WAVEFORMATEX_avilib wave_format_ex[AVI_MAX_TRACKS];
} avi_t;

#define AVI_MODE_WRITE  0
#define AVI_MODE_READ   1

// 错误码
#define AVI_ERR_SIZELIM      1     
#define AVI_ERR_OPEN         2     
#define AVI_ERR_READ         3     
#define AVI_ERR_WRITE        4     
#define AVI_ERR_WRITE_INDEX  5     
#define AVI_ERR_CLOSE        6     
#define AVI_ERR_NOT_PERM     7     
#define AVI_ERR_NO_MEM       8     
#define AVI_ERR_NO_AVI       9     
#define AVI_ERR_NO_HDRL     10     
#define AVI_ERR_NO_MOVI     11     
#define AVI_ERR_NO_VIDS     12     
#define AVI_ERR_NO_IDX      13     

// ------------------------- API 函数声明 -------------------------

// 读/写 打开与关闭
avi_t *AVI_open_input_file(const char *filename, int getIndex);
avi_t *AVI_open_output_file(char * filename);
int    AVI_close(avi_t *AVI);

// 写模式设置与写入
void   AVI_set_video(avi_t *AVI, int width, int height, double fps, char *compressor);
void   AVI_set_audio(avi_t *AVI, int channels, long rate, int bits, int format, long mp3rate);
int    AVI_write_frame(avi_t *AVI, char *data, long bytes, int keyframe);
int    AVI_write_audio(avi_t *AVI, char *data, long bytes);
int    AVI_append_audio(avi_t *AVI, char *data, long bytes);
int    AVI_dup_frame(avi_t *AVI);

// 读模式获取信息
long   AVI_video_frames(avi_t *AVI);
int    AVI_video_width(avi_t *AVI);
int    AVI_video_height(avi_t *AVI);
double AVI_frame_rate(avi_t *AVI);
char*  AVI_video_compressor(avi_t *AVI);
long   AVI_max_video_chunk(avi_t *AVI);

// 音频信息获取
int    AVI_audio_tracks(avi_t *AVI);
int    AVI_audio_channels(avi_t *AVI);
int    AVI_audio_bits(avi_t *AVI);
int    AVI_audio_format(avi_t *AVI);
long   AVI_audio_rate(avi_t *AVI);
long   AVI_audio_bytes(avi_t *AVI);
long   AVI_audio_chunks(avi_t *AVI);

// 读模式定位与读取
long   AVI_frame_size(avi_t *AVI, long frame);
int    AVI_seek_start(avi_t *AVI);
int    AVI_set_video_position(avi_t *AVI, long frame);
long   AVI_read_frame(avi_t *AVI, char *vidbuf, int *keyframe);
int    AVI_set_audio_position(avi_t *AVI, long byte);
long   AVI_read_audio(avi_t *AVI, char *audbuf, long bytes);
long   AVI_read_audio_chunk(avi_t *AVI, char *audbuf);
int    AVI_set_audio_track(avi_t *AVI, int track);
int    AVI_get_audio_track(avi_t *AVI);

// 其他
uint64_t AVI_max_size(void);
long   AVI_bytes_remain(avi_t *AVI);
long   AVI_bytes_written(avi_t *AVI);

#endif
