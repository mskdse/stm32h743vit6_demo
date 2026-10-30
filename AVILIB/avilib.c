#include "avilib.h"
#include <ctype.h> // 需要用到 tolower

int strncasecmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0) return 0;
    while (n-- != 0 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
        if (n == 0 || *s1 == '\0' || *s2 == '\0') break;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

long AVI_errno = 0;
#define FRAME_RATE_SCALE 1000000
#define AVI_MAX_LEN (0xFFFFFFFF - (1<<20)*16 - HEADERBYTES)
#define PAD_EVEN(x) ( ((x)+1) & ~1 )

// ==================== 核心静态内存定义 ====================
// 将 AVI 实例绑定到 D1 域，防止 SDMMC DMA 寻址异常
__attribute__((section(".RAM_D1"), aligned(4))) static avi_t g_avi_instance;
// AVI 头部缓存，写文件时使用
__attribute__((section(".RAM_D1"), aligned(4))) static unsigned char g_avi_header_buf[HEADERBYTES];
// 解析头部时的缓存
__attribute__((section(".RAM_D1"), aligned(4))) static unsigned char g_hdrl_data_buf[4096];

// ==================== 底层 FatFS 读写封装 ====================
static size_t avi_read(FIL* fp, char *buf, size_t len)
{
   UINT br;
   if (f_read(fp, buf, len, &br) != FR_OK) return 0;
   return br;
}

static size_t avi_write(FIL* fp, char *buf, size_t len)
{
   UINT bw;
   if (f_write(fp, buf, len, &bw) != FR_OK) return 0;
   return bw;
}

// ==================== 工具函数 ====================
static void long2str(unsigned char *dst, int n) 
{
   dst[0] = (n    )&0xff;
   dst[1] = (n>> 8)&0xff;
   dst[2] = (n>>16)&0xff;
   dst[3] = (n>>24)&0xff;
}

static unsigned long str2ulong(unsigned char *str) 
{
   return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}

static unsigned long str2ushort(unsigned char *str) 
{
   return ( str[0] | (str[1]<<8) );
}

static int avi_sampsize(avi_t *AVI, int j) 
{
   int s = ((AVI->track[j].a_bits+7)/8)*AVI->track[j].a_chans;
   if(s<4) s=4; 
   return s;
}

// ==================== 写文件逻辑 (录像) ====================

static int avi_add_index_entry(avi_t *AVI, unsigned char *tag, long flags, unsigned long pos, unsigned long len)
{
   if(AVI->n_idx >= MAX_AVI_FRAMES) 
	 {
       return -1; // 索引已满，防止越界
   }
   
   memcpy(AVI->idx[AVI->n_idx], tag, 4);
   long2str(AVI->idx[AVI->n_idx]+ 4, flags);
   long2str(AVI->idx[AVI->n_idx]+ 8, pos);
   long2str(AVI->idx[AVI->n_idx]+12, len);
   
   AVI->n_idx++;
   if(len > AVI->max_len) AVI->max_len = len;
   return 0;
}

static int avi_add_chunk(avi_t *AVI, unsigned char *tag, unsigned char *data, int length)
{
   unsigned char c[8];
   memcpy(c, tag, 4);
   long2str(c+4, length);
   length = PAD_EVEN(length);

   if( avi_write(&AVI->fdes, (char *)c, 8) != 8 ||
       avi_write(&AVI->fdes, (char *)data, length) != length )
   {
      f_lseek(&AVI->fdes, AVI->pos);
      AVI_errno = AVI_ERR_WRITE;
      return -1;
   }
   AVI->pos += 8 + length;
   return 0;
}

int avi_update_header(avi_t *AVI)
{
   int njunk, sampsize, hasIndex, ms_per_frame, frate, flag;
   int movi_len, hdrl_start, strl_start, j;
   long nhb;

   movi_len = AVI_MAX_LEN - HEADERBYTES + 4;
   hasIndex=1;

   if(AVI->fps < 0.001) 
	{
     frate=0; 
		 ms_per_frame=0;
   } 
	else 
	{
     frate = (int) (FRAME_RATE_SCALE*AVI->fps + 0.5);
     ms_per_frame=(int) (1000000/AVI->fps + 0.5);
   }

   nhb = 0;
   
   // 宏定义：写入 buffer
   #define OUT4CC(s) if(nhb<=HEADERBYTES-4) memcpy(g_avi_header_buf+nhb,s,4); nhb += 4
   #define OUTLONG(n) if(nhb<=HEADERBYTES-4) long2str(g_avi_header_buf+nhb,n); nhb += 4
   #define OUTSHRT(n) if(nhb<=HEADERBYTES-2) { g_avi_header_buf[nhb]=(n)&0xff; g_avi_header_buf[nhb+1]=((n)>>8)&0xff; } nhb += 2

   OUT4CC ("RIFF"); OUTLONG(movi_len); OUT4CC ("AVI ");
   OUT4CC ("LIST"); OUTLONG(0); hdrl_start = nhb; OUT4CC ("hdrl");
   
   #define AVIF_HASINDEX 0x00000010
   #define AVIF_MUSTUSEINDEX 0x00000020
   #define AVIF_ISINTERLEAVED 0x00000100

   OUT4CC ("avih"); OUTLONG(56); OUTLONG(ms_per_frame);
   OUTLONG(0); OUTLONG(0);
   flag = AVIF_ISINTERLEAVED | AVIF_HASINDEX;
   OUTLONG(flag); OUTLONG(0); OUTLONG(0); // 0 frames yet
   OUTLONG(AVI->anum+1); OUTLONG(0);
   OUTLONG(AVI->width); OUTLONG(AVI->height);
   OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(0);

   // Video Stream
   OUT4CC ("LIST"); OUTLONG(0); strl_start = nhb; OUT4CC ("strl");
   OUT4CC ("strh"); OUTLONG(56); OUT4CC ("vids"); OUT4CC (AVI->compressor);
   OUTLONG(0); OUTLONG(0); OUTLONG(0);
   OUTLONG(FRAME_RATE_SCALE); OUTLONG(frate); OUTLONG(0); OUTLONG(0);
   OUTLONG(0); OUTLONG(-1); OUTLONG(0); OUTLONG(0); OUTLONG(0);

   OUT4CC ("strf"); OUTLONG(40); OUTLONG(40);
   OUTLONG(AVI->width); OUTLONG(AVI->height);
   OUTSHRT(1); OUTSHRT(24); OUT4CC (AVI->compressor);
   OUTLONG(AVI->width*AVI->height*3); OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(0);
   long2str(g_avi_header_buf+strl_start-4, nhb-strl_start);

   // Audio Stream
   for(j=0; j<AVI->anum; ++j) {
       sampsize = avi_sampsize(AVI, j);
       OUT4CC ("LIST"); OUTLONG(0); strl_start = nhb; OUT4CC ("strl");
       OUT4CC ("strh"); OUTLONG(56); OUT4CC ("auds");
       OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(0);
       OUTLONG(sampsize/4); OUTLONG(1000*AVI->track[j].mp3rate/8);
       OUTLONG(0); OUTLONG(4*AVI->track[j].audio_bytes/sampsize);
       OUTLONG(0); OUTLONG(-1); OUTLONG(sampsize/4);
       OUTLONG(0); OUTLONG(0);
       OUT4CC ("strf"); OUTLONG(16);
       OUTSHRT(AVI->track[j].a_fmt); OUTSHRT(AVI->track[j].a_chans);
       OUTLONG(AVI->track[j].a_rate); OUTLONG(1000*AVI->track[j].mp3rate/8);
       OUTSHRT(sampsize/4); OUTSHRT(AVI->track[j].a_bits);
       long2str(g_avi_header_buf+strl_start-4, nhb-strl_start);
   }
   
   long2str(g_avi_header_buf+hdrl_start-4, nhb-hdrl_start);
   njunk = HEADERBYTES - nhb - 8 - 12;
   OUT4CC ("JUNK"); OUTLONG(njunk); memset(g_avi_header_buf+nhb,0,njunk); nhb += njunk;

   OUT4CC ("LIST"); OUTLONG(movi_len); OUT4CC ("movi");

   if ( f_lseek(&AVI->fdes, 0) != FR_OK ||
        avi_write(&AVI->fdes, (char *)g_avi_header_buf, HEADERBYTES) != HEADERBYTES ||
        f_lseek(&AVI->fdes, AVI->pos) != FR_OK )
   {
       AVI_errno = AVI_ERR_CLOSE;
       return -1;
   }
   #undef OUT4CC
   #undef OUTLONG
   #undef OUTSHRT
   return 0;
}

static int avi_close_output_file(avi_t *AVI)
{
   int ret, njunk, sampsize, hasIndex, ms_per_frame, frate, flag;
   unsigned long movi_len;
   int hdrl_start, strl_start, j;
   long nhb;

   movi_len = AVI->pos - HEADERBYTES + 4;
   
   // 写入索引
   ret = avi_add_chunk(AVI, (unsigned char *)"idx1", (unsigned char *)AVI->idx, AVI->n_idx*16);
   hasIndex = (ret==0);

   if(AVI->fps < 0.001) { frate=0; ms_per_frame=0; } 
   else {
     frate = (int) (FRAME_RATE_SCALE*AVI->fps + 0.5);
     ms_per_frame=(int) (1000000/AVI->fps + 0.5);
   }

   nhb = 0;
   #define OUT4CC(s) if(nhb<=HEADERBYTES-4) memcpy(g_avi_header_buf+nhb,s,4); nhb += 4
   #define OUTLONG(n) if(nhb<=HEADERBYTES-4) long2str(g_avi_header_buf+nhb,n); nhb += 4
   #define OUTSHRT(n) if(nhb<=HEADERBYTES-2) { g_avi_header_buf[nhb]=(n)&0xff; g_avi_header_buf[nhb+1]=((n)>>8)&0xff; } nhb += 2

   OUT4CC ("RIFF"); OUTLONG(AVI->pos - 8); OUT4CC ("AVI ");
   OUT4CC ("LIST"); OUTLONG(0); hdrl_start = nhb; OUT4CC ("hdrl");

   OUT4CC ("avih"); OUTLONG(56); OUTLONG(ms_per_frame);
   OUTLONG(0); OUTLONG(0);
   flag = AVIF_ISINTERLEAVED | AVIF_HASINDEX;
   OUTLONG(flag); OUTLONG(AVI->video_frames); OUTLONG(0); OUTLONG(AVI->anum+1);
   OUTLONG(0); OUTLONG(AVI->width); OUTLONG(AVI->height);
   OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(0);

   OUT4CC ("LIST"); OUTLONG(0); strl_start = nhb; OUT4CC ("strl");
   OUT4CC ("strh"); OUTLONG(56); OUT4CC ("vids"); OUT4CC (AVI->compressor);
   OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(FRAME_RATE_SCALE); OUTLONG(frate);
   OUTLONG(0); OUTLONG(AVI->video_frames); OUTLONG(0); OUTLONG(-1); 
   OUTLONG(0); OUTLONG(0); OUTLONG(0);

   OUT4CC ("strf"); OUTLONG(40); OUTLONG(40); OUTLONG(AVI->width); OUTLONG(AVI->height);
   OUTSHRT(1); OUTSHRT(24); OUT4CC (AVI->compressor);
   OUTLONG(AVI->width*AVI->height*3); OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(0);
   long2str(g_avi_header_buf+strl_start-4,nhb-strl_start);

   for(j=0; j<AVI->anum; ++j) {
       sampsize = avi_sampsize(AVI, j);
       OUT4CC ("LIST"); OUTLONG(0); strl_start = nhb; OUT4CC ("strl");
       OUT4CC ("strh"); OUTLONG(56); OUT4CC ("auds");
       OUTLONG(0); OUTLONG(0); OUTLONG(0); OUTLONG(sampsize/4);
       OUTLONG(1000*AVI->track[j].mp3rate/8); OUTLONG(0);
       OUTLONG(4*AVI->track[j].audio_bytes/sampsize);
       OUTLONG(0); OUTLONG(-1); OUTLONG(sampsize/4); OUTLONG(0); OUTLONG(0);
       OUT4CC ("strf"); OUTLONG(16); OUTSHRT(AVI->track[j].a_fmt); OUTSHRT(AVI->track[j].a_chans);
       OUTLONG(AVI->track[j].a_rate); OUTLONG(1000*AVI->track[j].mp3rate/8);
       OUTSHRT(sampsize/4); OUTSHRT(AVI->track[j].a_bits);
       long2str(g_avi_header_buf+strl_start-4,nhb-strl_start);
   }
   long2str(g_avi_header_buf+hdrl_start-4,nhb-hdrl_start);

   // Info List
   OUT4CC ("LIST"); OUTLONG(64+12); OUT4CC ("INFO");
   OUT4CC ("ISFT"); OUTLONG(64); memset(g_avi_header_buf+nhb, 0, 64); nhb += 64;

   njunk = HEADERBYTES - nhb - 8 - 12;
   OUT4CC ("JUNK"); OUTLONG(njunk); memset(g_avi_header_buf+nhb,0,njunk); nhb += njunk;
   OUT4CC ("LIST"); OUTLONG(movi_len); OUT4CC ("movi");

   if ( f_lseek(&AVI->fdes, 0) != FR_OK ||
        avi_write(&AVI->fdes, (char *)g_avi_header_buf, HEADERBYTES) != HEADERBYTES ||
        f_lseek(&AVI->fdes, AVI->pos) != FR_OK )
   {
      AVI_errno = AVI_ERR_CLOSE;
      return -1;
   }
   return 0;
}

avi_t* AVI_open_output_file(char * filename)
{
   avi_t *AVI = &g_avi_instance;
   memset((void *)AVI, 0, sizeof(avi_t));
   
   FRESULT res = f_open(&AVI->fdes, filename, FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
   if (res != FR_OK) {
       AVI_errno = AVI_ERR_OPEN;
       return 0;
   }

   memset(g_avi_header_buf, 0, HEADERBYTES);
   if (avi_write(&AVI->fdes, (char *)g_avi_header_buf, HEADERBYTES) != HEADERBYTES) {
      f_close(&AVI->fdes); 
      AVI_errno = AVI_ERR_WRITE;
      return 0;
   }

   AVI->pos  = HEADERBYTES;
   AVI->mode = AVI_MODE_WRITE; 
   return AVI;
}

static int avi_write_data(avi_t *AVI, char *data, unsigned long length, int audio, int keyframe)
{
   int n;
   unsigned char astr[5];

   if ( (AVI->pos + 8 + length + 8 + (AVI->n_idx+1)*16) > AVI_MAX_LEN ) return -1;
   
   sprintf((char *)astr, "0%1dwb", (int)(AVI->aptr+1));
   if(audio) n = avi_add_index_entry(AVI,astr,0x00,AVI->pos,length);
   else      n = avi_add_index_entry(AVI,(unsigned char *)"00db",((keyframe)?0x10:0x0),AVI->pos,length);
   
   if(n) return -1;
   
   if(audio) n = avi_add_chunk(AVI,astr,(unsigned char *)data,length);
   else      n = avi_add_chunk(AVI,(unsigned char *)"00db",(unsigned char *)data,length);
   
   if (n) return -1;
   return 0;
}

void AVI_set_video(avi_t *AVI, int width, int height, double fps, char *compressor)
{
   if(AVI->mode==AVI_MODE_READ) return;
   AVI->width = width; AVI->height = height; AVI->fps = fps;
   if(strncmp(compressor, "RGB", 3)==0) memset(AVI->compressor, 0, 4);
   else memcpy(AVI->compressor, compressor, 4);     
   AVI->compressor[4] = 0;
   avi_update_header(AVI);
}

void AVI_set_audio(avi_t *AVI, int channels, long rate, int bits, int format, long mp3rate)
{
   if(AVI->mode==AVI_MODE_READ || AVI->anum >= AVI_MAX_TRACKS) return;
   AVI->aptr = AVI->anum++;
   AVI->track[AVI->aptr].a_chans = channels;
   AVI->track[AVI->aptr].a_rate  = rate;
   AVI->track[AVI->aptr].a_bits  = bits;
   AVI->track[AVI->aptr].a_fmt   = format;
   AVI->track[AVI->aptr].mp3rate = mp3rate;
   avi_update_header(AVI);
}

int AVI_write_frame(avi_t *AVI, char *data, long bytes, int keyframe)
{
  if(AVI->mode==AVI_MODE_READ) return -1;
  unsigned long pos = AVI->pos;
  if(avi_write_data(AVI,data,bytes,0,keyframe)) return -1;
  AVI->last_pos = pos; AVI->last_len = bytes; AVI->video_frames++;
  return 0;
}

int AVI_dup_frame(avi_t *AVI)
{
   if(AVI->mode==AVI_MODE_READ) return -1;
   if(AVI->last_pos==0) return 0; 
   if(avi_add_index_entry(AVI,(unsigned char *)"00db",0x10,AVI->last_pos,AVI->last_len)) return -1;
   AVI->video_frames++; AVI->must_use_index = 1;
   return 0;
}

int AVI_write_audio(avi_t *AVI, char *data, long bytes)
{
   if(AVI->mode==AVI_MODE_READ) return -1;
   if( avi_write_data(AVI,data,bytes,1,0) ) return -1;
   AVI->track[AVI->aptr].audio_bytes += bytes;
   return 0;
}

int AVI_append_audio(avi_t *AVI, char *data, long bytes)
{
  if(AVI->mode==AVI_MODE_READ) return -1;
  long i, length, pos;
  unsigned char c[4];
  --AVI->n_idx;
  length = str2ulong(AVI->idx[AVI->n_idx]+12);
  pos    = str2ulong(AVI->idx[AVI->n_idx]+8);
  long2str(AVI->idx[AVI->n_idx]+12,length+bytes);   
  ++AVI->n_idx;
  AVI->track[AVI->aptr].audio_bytes += bytes;
  f_lseek(&AVI->fdes, pos+4);
  long2str(c, length+bytes);     
  avi_write(&AVI->fdes, (char *)c, 4);
  f_lseek(&AVI->fdes, pos+8+length);
  i=PAD_EVEN(length + bytes);
  bytes = i - length;
  avi_write(&AVI->fdes, data, bytes);
  AVI->pos = pos + 8 + i;
  return 0;
}

// ==================== 读文件逻辑 (播放) ====================

avi_t *AVI_open_input_file(const char *filename, int getIndex)
{
  avi_t *AVI = &g_avi_instance;
  memset((void *)AVI, 0, sizeof(avi_t));
  AVI->mode = AVI_MODE_READ;
  
  FRESULT res = f_open(&AVI->fdes, filename, FA_READ);
  if (res != FR_OK) { AVI_errno = AVI_ERR_OPEN; return 0; }
  
  extern int avi_parse_input_file(avi_t *AVI, int getIndex);
  avi_parse_input_file(AVI, getIndex);
  AVI->aptr = 0; 
  return AVI;
}

int avi_parse_input_file(avi_t *AVI, int getIndex)
{
  long i, rate, scale, idx_type;
  off_t n;
  unsigned char *hdrl_data = 0;
  long header_offset=0, hdrl_len=0;
  long nvi, nai[AVI_MAX_TRACKS], ioff;
  long tot[AVI_MAX_TRACKS];
  int j, lasttag = 0, vids_strh_seen = 0, vids_strf_seen = 0, auds_strh_seen = 0, num_stream = 0;
  char data[256];

  if( avi_read(&AVI->fdes,data,12) != 12 ) return -1;
  if( strncasecmp(data,"RIFF",4)!=0 || strncasecmp(data+8,"AVI ",4)!=0 ) return -1;

  while(1) {
      if( avi_read(&AVI->fdes,data,8) != 8 ) break; 
      n = PAD_EVEN(str2ulong((unsigned char *)data+4));

      if(strncasecmp(data,"LIST",4) == 0) {
         if( avi_read(&AVI->fdes,data,4) != 4 ) return -1;
         n -= 4;
         if(strncasecmp(data,"hdrl",4) == 0) {
            hdrl_len = n; hdrl_data = g_hdrl_data_buf;
            if (n > sizeof(g_hdrl_data_buf)) return -1; 
            header_offset = AVI->fdes.fptr;
            if( avi_read(&AVI->fdes,(char *)hdrl_data,n) != n ) return -1;
         } else if(strncasecmp(data,"movi",4) == 0) {
            AVI->movi_start = AVI->fdes.fptr;
            f_lseek(&AVI->fdes, AVI->fdes.fptr + n);
         } else f_lseek(&AVI->fdes, AVI->fdes.fptr + n);
      } else if(strncasecmp(data,"idx1",4) == 0) {
         AVI->n_idx = AVI->max_idx = n/16;
         if(AVI->n_idx > MAX_AVI_FRAMES) AVI->n_idx = MAX_AVI_FRAMES;
         if(avi_read(&AVI->fdes, (char *)AVI->idx, AVI->n_idx*16) != AVI->n_idx*16 ) return -1;
      } else f_lseek(&AVI->fdes, AVI->fdes.fptr + n);
   }

   if(!hdrl_data || !AVI->movi_start) return -1;

   for(i=0; i<hdrl_len;) {
      if(strncasecmp((char *)hdrl_data+i,"LIST",4)==0) { i+= 12; continue; }
      n = PAD_EVEN(str2ulong(hdrl_data+i+4));

      if(strncasecmp((char *)hdrl_data+i,"strh",4)==0) {
         i += 8;
         if(strncasecmp((char *)hdrl_data+i,"vids",4)==0 && !vids_strh_seen) {
            memcpy(AVI->compressor,hdrl_data+i+4,4); AVI->compressor[4] = 0;
            AVI->v_codech_off = header_offset + i+4;
            scale = str2ulong(hdrl_data+i+20); rate = str2ulong(hdrl_data+i+24);
            if(scale!=0) AVI->fps = (double)rate/(double)scale;
            AVI->video_frames = str2ulong(hdrl_data+i+32); AVI->video_strn = num_stream;
            AVI->max_len = 0; vids_strh_seen = 1; lasttag = 1; 
         } else if (strncasecmp((char *)hdrl_data+i,"auds",4)==0 && !auds_strh_seen) {
            AVI->aptr = AVI->anum++;
            if(AVI->anum > AVI_MAX_TRACKS) return -1;
            AVI->track[AVI->aptr].audio_bytes = str2ulong(hdrl_data+i+32)*avi_sampsize(AVI, 0);
            AVI->track[AVI->aptr].audio_strn = num_stream; lasttag = 2;
            AVI->track[AVI->aptr].a_codech_off = header_offset + i;
         } else lasttag = 0;
         num_stream++;
      } else if(strncasecmp((char *)hdrl_data+i,"strf",4)==0) {
         i += 8;
         if(lasttag == 1) {
            memcpy(&(AVI->bitmap_info_header), hdrl_data + i, sizeof(BITMAPINFOHEADER_avilib));
            AVI->width = str2ulong(hdrl_data+i+4); AVI->height = str2ulong(hdrl_data+i+8);
            vids_strf_seen = 1; AVI->v_codecf_off = header_offset + i+16;
            memcpy(AVI->compressor2, hdrl_data+i+16, 4); AVI->compressor2[4] = 0;
         } else if(lasttag == 2) {
            memcpy(&(AVI->wave_format_ex[AVI->aptr]), hdrl_data + i, sizeof(WAVEFORMATEX_avilib));
            AVI->track[AVI->aptr].a_fmt = str2ushort(hdrl_data+i);
            AVI->track[AVI->aptr].a_codecf_off = header_offset + i;
            AVI->track[AVI->aptr].a_chans = str2ushort(hdrl_data+i+2);
            AVI->track[AVI->aptr].a_rate = str2ulong(hdrl_data+i+4);
            AVI->track[AVI->aptr].mp3rate = 8*str2ulong(hdrl_data+i+8)/1000;
            AVI->track[AVI->aptr].a_bits = str2ushort(hdrl_data+i+14);
         }
         lasttag = 0;
      } else { i += 8; lasttag = 0; }
      i += n;
   }
   if(!vids_strh_seen || !vids_strf_seen) return -1;

   AVI->video_tag[0] = AVI->video_strn/10 + '0'; AVI->video_tag[1] = AVI->video_strn%10 + '0';
   AVI->video_tag[2] = 'd'; AVI->video_tag[3] = 'b';
   if(!AVI->track[0].a_chans) AVI->track[0].audio_strn = 99;
   for(j=0; j<AVI->anum; ++j) {
     AVI->track[j].audio_tag[0] = (j+1)/10 + '0'; AVI->track[j].audio_tag[1] = (j+1)%10 + '0';
     AVI->track[j].audio_tag[2] = 'w'; AVI->track[j].audio_tag[3] = 'b';
   }

   f_lseek(&AVI->fdes,AVI->movi_start);
   if(!getIndex) return(0);

   idx_type = 0;
   if(AVI->n_idx > 0) {
      off_t pos, len;
      for(i=0; i<AVI->n_idx; i++)
         if( strncasecmp((char *)AVI->idx[i],(char *)AVI->video_tag,3)==0 ) break;
      if(i >= AVI->n_idx) return -1;
      pos = str2ulong(AVI->idx[i]+ 8); len = str2ulong(AVI->idx[i]+12);
      f_lseek(&AVI->fdes,pos);
      if(avi_read(&AVI->fdes,data,8)!=8) return -1;
      if( strncasecmp(data,(char *)AVI->idx[i],4)==0 && str2ulong((unsigned char *)data+4)==len ) idx_type = 1; 
      else {
         f_lseek(&AVI->fdes,pos+AVI->movi_start-4);
         if(avi_read(&AVI->fdes,data,8)!=8) return -1;
         if( strncasecmp(data,(char *)AVI->idx[i],4)==0 && str2ulong((unsigned char *)data+4)==len ) idx_type = 2; 
      }
   }

   if(idx_type == 0) {
      f_lseek(&AVI->fdes, AVI->movi_start); AVI->n_idx = 0;
      while(1) {
         if( avi_read(&AVI->fdes,data,8) != 8 ) break;
         n = str2ulong((unsigned char *)data+4);
         if(strncasecmp(data,"LIST",4)==0) { f_lseek(&AVI->fdes, AVI->fdes.fptr + 4); continue; }
         if( ( (data[2]=='d'||data[2]=='D') && (data[3]=='b'||data[3]=='B'||data[3]=='c'||data[3]=='C') ) ||
             ( (data[2]=='w'||data[2]=='W') && (data[3]=='b'||data[3]=='B') ) ) {
             avi_add_index_entry(AVI,(unsigned char *)data,0,AVI->fdes.fptr - 8, n);
         }
         f_lseek(&AVI->fdes, AVI->fdes.fptr + PAD_EVEN(n));
      }
      idx_type = 1;
   }

   nvi = 0;
   for(j=0; j<AVI->anum; ++j) nai[j] = 0;
   for(i=0;i<AVI->n_idx;i++) {
     if(strncasecmp((char *)AVI->idx[i],AVI->video_tag,3) == 0) nvi++;
     for(j=0; j<AVI->anum; ++j) if(strncasecmp((char *)AVI->idx[i], AVI->track[j].audio_tag,4) == 0) nai[j]++;
   }
   AVI->video_frames = nvi;
   for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_chunks = nai[j];
   if(AVI->video_frames==0) return -1;
   
   nvi = 0;
   for(j=0; j<AVI->anum; ++j) nai[j] = tot[j] = 0;
   ioff = idx_type == 1 ? 8 : AVI->movi_start+4;
   
   for(i=0; i<AVI->n_idx; i++) {
     if(strncasecmp((char *)AVI->idx[i],AVI->video_tag,3) == 0 && nvi < MAX_AVI_FRAMES) {
       AVI->video_index[nvi].key = str2ulong(AVI->idx[i]+ 4);
       AVI->video_index[nvi].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
       AVI->video_index[nvi].len = str2ulong(AVI->idx[i]+12);
       nvi++;
     }
     for(j=0; j<AVI->anum; ++j) {
       if(strncasecmp((char *)AVI->idx[i],AVI->track[j].audio_tag,4) == 0 && nai[j] < MAX_AVI_AUDIO_CHUNKS) {
         AVI->track[j].audio_index[nai[j]].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
         AVI->track[j].audio_index[nai[j]].len = str2ulong(AVI->idx[i]+12);
         AVI->track[j].audio_index[nai[j]].tot = tot[j];
         tot[j] += AVI->track[j].audio_index[nai[j]].len;
         nai[j]++;
       }
     }
   }
   for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_bytes = tot[j];
   f_lseek(&AVI->fdes,AVI->movi_start);
   AVI->video_pos = 0;
   return(0);
}

int AVI_close(avi_t *AVI)
{
   int ret = 0;
   if(AVI->mode == AVI_MODE_WRITE) ret = avi_close_output_file(AVI);
   f_close(&AVI->fdes);
   return ret;
}

// ==================== 其他 API 实现 ====================
long AVI_video_frames(avi_t *AVI) { return AVI->video_frames; }
int  AVI_video_width(avi_t *AVI) { return AVI->width; }
int  AVI_video_height(avi_t *AVI) { return AVI->height; }
double AVI_frame_rate(avi_t *AVI) { return AVI->fps; }
char* AVI_video_compressor(avi_t *AVI) { return AVI->compressor2; }
long AVI_max_video_chunk(avi_t *AVI) { return AVI->max_len; }
int  AVI_audio_tracks(avi_t *AVI) { return AVI->anum; }
int  AVI_audio_channels(avi_t *AVI) { return AVI->track[AVI->aptr].a_chans; }
int  AVI_audio_bits(avi_t *AVI) { return AVI->track[AVI->aptr].a_bits; }
int  AVI_audio_format(avi_t *AVI) { return AVI->track[AVI->aptr].a_fmt; }
long AVI_audio_rate(avi_t *AVI) { return AVI->track[AVI->aptr].a_rate; }
long AVI_audio_bytes(avi_t *AVI) { return AVI->track[AVI->aptr].audio_bytes; }
long AVI_audio_chunks(avi_t *AVI) { return AVI->track[AVI->aptr].audio_chunks; }
long AVI_audio_mp3rate(avi_t *AVI) { return AVI->track[AVI->aptr].mp3rate; }
int  AVI_set_audio_track(avi_t *AVI, int track) { if(track<0 || track>=AVI->anum) return -1; AVI->aptr=track; return 0; }
int  AVI_get_audio_track(avi_t *AVI) { return AVI->aptr; }

long AVI_frame_size(avi_t *AVI, long frame) {
   if(AVI->mode==AVI_MODE_WRITE || frame < 0 || frame >= AVI->video_frames) return 0;
   return(AVI->video_index[frame].len);
}

int AVI_seek_start(avi_t *AVI) {
   if(AVI->mode==AVI_MODE_WRITE) return -1;
   f_lseek(&AVI->fdes,AVI->movi_start);
   AVI->video_pos = 0;
   return 0;
}

int AVI_set_video_position(avi_t *AVI, long frame) {
   if(AVI->mode==AVI_MODE_WRITE) return -1;
   if (frame < 0 ) frame = 0;
   AVI->video_pos = frame;
   return 0;
}

long AVI_read_frame(avi_t *AVI, char *vidbuf, int *keyframe) {
   long n;
   if(AVI->mode==AVI_MODE_WRITE || AVI->video_pos < 0 || AVI->video_pos >= AVI->video_frames) return -1;
   n = AVI->video_index[AVI->video_pos].len;
   *keyframe = (AVI->video_index[AVI->video_pos].key==0x10) ? 1:0;
   f_lseek(&AVI->fdes, AVI->video_index[AVI->video_pos].pos);
   if (avi_read(&AVI->fdes,vidbuf,n) != n) { AVI_errno = AVI_ERR_READ; return -1; }
   AVI->video_pos++;
   return n;
}

int AVI_set_audio_position(avi_t *AVI, long byte) {
   long n0, n1, n;
   if(AVI->mode==AVI_MODE_WRITE) return -1;
   if(byte < 0) byte = 0;
   n0 = 0; n1 = AVI->track[AVI->aptr].audio_chunks;
   while(n0<n1-1) {
      n = (n0+n1)/2;
      if(AVI->track[AVI->aptr].audio_index[n].tot>byte) n1 = n; else n0 = n;
   }
   AVI->track[AVI->aptr].audio_posc = n0;
   AVI->track[AVI->aptr].audio_posb = byte - AVI->track[AVI->aptr].audio_index[n0].tot;
   return 0;
}

long AVI_read_audio(avi_t *AVI, char *audbuf, long bytes) {
   long nr = 0, pos, left, todo;
   if(AVI->mode==AVI_MODE_WRITE) return -1;
   while(bytes>0) {
      left = AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].len - AVI->track[AVI->aptr].audio_posb;
      if(left==0) {
         if(AVI->track[AVI->aptr].audio_posc>=AVI->track[AVI->aptr].audio_chunks-1) return nr;
         AVI->track[AVI->aptr].audio_posc++;
         AVI->track[AVI->aptr].audio_posb = 0;
         continue;
      }
      todo = (bytes < left) ? bytes : left;
      pos = AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].pos + AVI->track[AVI->aptr].audio_posb;
      f_lseek(&AVI->fdes, pos);
      if (avi_read(&AVI->fdes,audbuf+nr,todo) != todo) { AVI_errno = AVI_ERR_READ; return -1; }
      bytes -= todo; nr += todo;
      AVI->track[AVI->aptr].audio_posb += todo;
   }
   return nr;
}

long AVI_read_audio_chunk(avi_t *AVI, char *audbuf) {
   long pos, left;
   if(AVI->mode==AVI_MODE_WRITE) return -1;
   if (AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].len == 0) return 0;
   left = AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].len - AVI->track[AVI->aptr].audio_posb;
   if (audbuf == NULL) return left;
   if(left==0) return 0;
   pos = AVI->track[AVI->aptr].audio_index[AVI->track[AVI->aptr].audio_posc].pos + AVI->track[AVI->aptr].audio_posb;
   f_lseek(&AVI->fdes, pos);
   if (avi_read(&AVI->fdes,audbuf,left) != left) { AVI_errno = AVI_ERR_READ; return -1; }
   AVI->track[AVI->aptr].audio_posc++;
   AVI->track[AVI->aptr].audio_posb = 0;
   return left;
}

uint64_t AVI_max_size(void) { return((uint64_t) AVI_MAX_LEN); }
long AVI_bytes_remain(avi_t *AVI) { return (AVI->mode==AVI_MODE_READ) ? 0 : (AVI_MAX_LEN - (AVI->pos + 8 + 16*AVI->n_idx)); }
long AVI_bytes_written(avi_t *AVI) { return (AVI->mode==AVI_MODE_READ) ? 0 : (AVI->pos + 8 + 16*AVI->n_idx); }
