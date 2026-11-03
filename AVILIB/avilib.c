#include "avilib.h"
#include <string.h>
#include <limits.h>

#define AVI_FRAME_RATE_SCALE      1000000UL
#define AVIIF_KEYFRAME            0x00000010UL
#define AVIF_HASINDEX             0x00000010UL
#define AVIF_ISINTERLEAVED        0x00000100UL
#define AVI_MAX_FILE_SIZE         (0xFFFFFFFFUL - 0x00100000UL)
#define AVI_MAX_RECURSION         4U

long AVI_errno = AVI_ERR_NONE;

/*
 * One AVI object is intentionally used. It occupies about 290 KiB with the
 * default index limits. Keep it and the writer header buffer in AXI SRAM D1.
 */
__attribute__((section(".RAM_D1"), aligned(4))) static avi_t g_avi_instance;
__attribute__((section(".RAM_D1"), aligned(4))) static uint8_t g_avi_header_buf[HEADERBYTES];
__attribute__((section(".RAM_D1"), aligned(4))) static uint8_t g_avi_idx_buffer[512];

typedef struct {
    char type[4];
    char handler[4];
    uint32_t scale;
    uint32_t rate;
    uint32_t start;
    uint32_t length;
    uint32_t suggested_size;
    uint32_t sample_size;
    uint8_t have_strh;
    uint8_t have_strf;
    uint8_t strf[64];
    uint32_t strf_size;
    avi_off_t strh_pos;
    avi_off_t strf_pos;
} avi_stream_tmp_t;

typedef struct {
    uint32_t pos;
    uint32_t len;
    uint32_t flags;
    char tag[4];
} avi_idx_tmp_t;

/* Compile-time guard: the library's static AVI storage must stay below 450 KiB. */
typedef char avi_static_memory_guard[(sizeof(avi_t) + sizeof(g_avi_header_buf) + sizeof(g_avi_idx_buffer) < AVI_STATIC_LIMIT_BYTES) ? 1 : -1];

static uint16_t avi_get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t avi_get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void avi_put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void avi_put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint64_t avi_pad_even64(uint32_t n)
{
    return (((uint64_t)n + 1ULL) & ~1ULL);
}

static int avi_tag_equal(const void *aa, const char *b)
{
    const uint8_t *a = (const uint8_t *)aa;
    return (a[0] == (uint8_t)b[0] && a[1] == (uint8_t)b[1] &&
            a[2] == (uint8_t)b[2] && a[3] == (uint8_t)b[3]);
}

static int avi_tag_equal_ci(const void *aa, const char *b)
{
    const uint8_t *a = (const uint8_t *)aa;
    unsigned int i;
    for(i = 0; i < 4; i++) {
        char ca = (char)a[i], cb = b[i];
        if(ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
        if(cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
        if(ca != cb) return 0;
    }
    return 1;
}

static int avi_read_exact(FIL *fp, void *buf, uint32_t len)
{
    UINT br = 0;
    if(len == 0U) return 1;
    if(f_read(fp, buf, (UINT)len, &br) != FR_OK) return 0;
    return (br == (UINT)len);
}

static int avi_write_exact(FIL *fp, const void *buf, uint32_t len)
{
    UINT bw = 0;
    if(len == 0U) return 1;
    if(f_write(fp, buf, (UINT)len, &bw) != FR_OK) return 0;
    return (bw == (UINT)len);
}

static int avi_seek(FIL *fp, uint32_t pos)
{
    return (f_lseek(fp, (FSIZE_t)pos) == FR_OK);
}

static int avi_read_at(FIL *fp, uint32_t pos, void *buf, uint32_t len)
{
    if(!avi_seek(fp, pos)) return 0;
    return avi_read_exact(fp, buf, len);
}

static int avi_get_chunk(FIL *fp, uint32_t pos, uint32_t parent_end,
                         uint8_t header[8], uint32_t *data_pos,
                         uint32_t *raw_end, uint32_t *next_pos)
{
    uint32_t size;
    uint64_t end64, next64;

    if(pos > parent_end || parent_end - pos < 8U) return 0;
    if(!avi_read_at(fp, pos, header, 8U)) return 0;

    size = avi_get_u32(header + 4);
    end64 = (uint64_t)pos + 8ULL + size;
    next64 = (uint64_t)pos + 8ULL + avi_pad_even64(size);
    if(end64 > parent_end || next64 > parent_end) return 0;

    *data_pos = pos + 8U;
    *raw_end = (uint32_t)end64;
    *next_pos = (uint32_t)next64;
    return 1;
}

static int avi_make_stream_tag(long stream_no, char a, char b, char tag[4])
{
    if(stream_no < 0 || stream_no > 99) return 0;
    tag[0] = (char)('0' + (stream_no / 10));
    tag[1] = (char)('0' + (stream_no % 10));
    tag[2] = a;
    tag[3] = b;
    return 1;
}

static int avi_is_video_chunk(const avi_t *AVI, const char tag[4])
{
    if(tag[0] != AVI->video_tag[0] || tag[1] != AVI->video_tag[1]) return 0;
    return ((tag[2] == 'd' || tag[2] == 'D') &&
            (tag[3] == 'b' || tag[3] == 'B' || tag[3] == 'c' || tag[3] == 'C'));
}

static int avi_audio_track_from_tag(const avi_t *AVI, const char tag[4])
{
    int i;
    if(AVI->anum < 0 || AVI->anum > (int)AVI_MAX_TRACKS) return -1;
    for(i = 0; i < (int)AVI_MAX_TRACKS; i++) {
        if(i >= AVI->anum) break;
        if(avi_tag_equal_ci(tag, AVI->track[i].audio_tag)) return i;
    }
    return -1;
}

static int avi_add_video_index(avi_t *AVI, uint32_t data_pos, uint32_t len,
                               uint32_t flags, const char tag[4])
{
    uint32_t n = AVI->video_chunk_count;
    if(n >= MAX_AVI_FRAMES) {
        AVI_errno = AVI_ERR_INDEX_LIMIT;
        return 0;
    }
    AVI->video_index[n].pos = data_pos;
    AVI->video_index[n].len = len;
    AVI->video_index[n].key = flags;
    AVI->video_chunk_count++;
    AVI->video_frames = (long)AVI->video_chunk_count;
    if(len > AVI->max_len) AVI->max_len = len;
    if(n == 0U || (AVI->video_tag[2] == 0)) memcpy(AVI->video_tag, tag, 4);
    return 1;
}

static int avi_add_audio_index(avi_t *AVI, int track, uint32_t data_pos,
                               uint32_t len)
{
    track_t *t;
    uint32_t n;
    uint64_t total;

    if(track < 0 || track >= AVI->anum) return 1;
    t = &AVI->track[track];
    n = (uint32_t)t->audio_chunks;
    if(n >= MAX_AVI_AUDIO_CHUNKS) {
        AVI_errno = AVI_ERR_INDEX_LIMIT;
        return 0;
    }
    total = (uint64_t)t->audio_bytes + len;
    if(total > 0xFFFFFFFFULL) {
        AVI_errno = AVI_ERR_SIZELIM;
        return 0;
    }
    t->audio_index[n].pos = data_pos;
    t->audio_index[n].len = len;
    t->audio_index[n].tot = t->audio_bytes;
    t->audio_chunks++;
    t->audio_bytes = (uint32_t)total;
    if(len > t->max_chunk) t->max_chunk = len;
    return 1;
}

static void avi_reset_indexes(avi_t *AVI)
{
    int i;
    AVI->video_chunk_count = 0;
    AVI->video_frames = 0;
    AVI->max_len = 0;
    AVI->n_idx = 0;
    AVI->video_pos = 0;
    for(i = 0; i < (int)AVI_MAX_TRACKS; i++) {
        AVI->track[i].audio_chunks = 0;
        AVI->track[i].audio_bytes = 0;
        AVI->track[i].audio_posc = 0;
        AVI->track[i].audio_posb = 0;
        AVI->track[i].max_chunk = 0;
    }
}

static void avi_parse_bitmap_info(avi_t *AVI, const uint8_t *p, uint32_t len)
{
    BITMAPINFOHEADER_avilib *b = &AVI->bitmap_info_header;
    if(len < 40U) return;
    b->bi_size = avi_get_u32(p + 0);
    b->bi_width = (int32_t)avi_get_u32(p + 4);
    b->bi_height = (int32_t)avi_get_u32(p + 8);
    b->bi_planes = avi_get_u16(p + 12);
    b->bi_bit_count = avi_get_u16(p + 14);
    b->bi_compression = avi_get_u32(p + 16);
    b->bi_size_image = avi_get_u32(p + 20);
    b->bi_x_pels_per_meter = (int32_t)avi_get_u32(p + 24);
    b->bi_y_pels_per_meter = (int32_t)avi_get_u32(p + 28);
    b->bi_clr_used = avi_get_u32(p + 32);
    b->bi_clr_important = avi_get_u32(p + 36);

    AVI->width = b->bi_width;
    AVI->height = (b->bi_height < 0) ? -(long)b->bi_height : b->bi_height;
    memcpy(AVI->compressor2, p + 16, 4);
    AVI->compressor2[4] = 0;
}

static void avi_parse_wave_format(avi_t *AVI, int track, const uint8_t *p,
                                  uint32_t len)
{
    WAVEFORMATEX_avilib *w;
    track_t *t;
    if(track < 0 || (unsigned int)track >= AVI_MAX_TRACKS || len < 16U) return;

    w = &AVI->wave_format_ex[track];
    t = &AVI->track[track];
    w->w_format_tag = avi_get_u16(p + 0);
    w->n_channels = avi_get_u16(p + 2);
    w->n_samples_per_sec = avi_get_u32(p + 4);
    w->n_avg_bytes_per_sec = avi_get_u32(p + 8);
    w->n_block_align = avi_get_u16(p + 12);
    w->w_bits_per_sample = avi_get_u16(p + 14);
    w->cb_size = (len >= 18U) ? avi_get_u16(p + 16) : 0U;

    t->a_fmt = w->w_format_tag;
    t->a_chans = w->n_channels;
    t->a_rate = w->n_samples_per_sec;
    t->a_bits = w->w_bits_per_sample;
    t->avg_bytes_per_sec = w->n_avg_bytes_per_sec;
    t->block_align = w->n_block_align;
    t->cb_size = w->cb_size;
    t->mp3rate = (long)(((uint64_t)w->n_avg_bytes_per_sec * 8ULL) / 1000ULL);
}

static int avi_parse_strl(avi_t *AVI, uint32_t start, uint32_t end,
                          uint32_t stream_no)
{
    uint32_t pos = start, data_pos, raw_end, next_pos;
    uint8_t h[8], b[64];
    avi_stream_tmp_t s;
    int have_stream = 0;

    memset(&s, 0, sizeof(s));
    while(pos < end) {
        uint32_t size;
        if(!avi_get_chunk(&AVI->fdes, pos, end, h, &data_pos, &raw_end, &next_pos)) {
            AVI_errno = AVI_ERR_NO_HDRL;
            return 0;
        }
        size = avi_get_u32(h + 4);
        if(avi_tag_equal(h, "strh")) {
            if(size < 56U || !avi_read_at(&AVI->fdes, data_pos, b, 56U)) {
                AVI_errno = AVI_ERR_NO_HDRL;
                return 0;
            }
            memcpy(s.type, b, 4);
            memcpy(s.handler, b + 4, 4);
            s.scale = avi_get_u32(b + 20);
            s.rate = avi_get_u32(b + 24);
            s.start = avi_get_u32(b + 28);
            s.length = avi_get_u32(b + 32);
            s.suggested_size = avi_get_u32(b + 36);
            s.sample_size = avi_get_u32(b + 44);
            s.strh_pos = pos;
            s.have_strh = 1;
        } else if(avi_tag_equal(h, "strf")) {
            uint32_t copy_len = size;
            if(copy_len > sizeof(s.strf)) copy_len = sizeof(s.strf);
            memset(s.strf, 0, sizeof(s.strf));
            if(copy_len && !avi_read_at(&AVI->fdes, data_pos, s.strf, copy_len)) {
                AVI_errno = AVI_ERR_READ;
                return 0;
            }
            s.strf_size = size;
            s.strf_pos = pos;
            s.have_strf = 1;
        }
        pos = next_pos;
    }

    if(!s.have_strh) return 1;
    have_stream = 1;
    if(avi_tag_equal_ci(s.type, "vids")) {
        if(AVI->video_strn >= 0) return 1; /* Keep the first video stream. */
        AVI->video_strn = (long)stream_no;
        AVI->video_stream_scale = s.scale;
        AVI->video_stream_rate = s.rate;
        AVI->video_stream_length = s.length;
        AVI->header_video_frames = s.length;
        AVI->video_stream_sample_size = s.sample_size;
        AVI->video_stream_suggested_size = s.suggested_size;
        memcpy(AVI->compressor, s.handler, 4);
        AVI->compressor[4] = 0;
        AVI->v_codech_off = s.strh_pos;
        if(s.have_strf && s.strf_size >= 40U) {
            avi_parse_bitmap_info(AVI, s.strf, s.strf_size);
            AVI->v_codecf_off = s.strf_pos + 8U + 16U;
        }
        if(s.scale && s.rate) AVI->fps = (double)s.rate / (double)s.scale;
        AVI->video_is_compressed = (AVI->bitmap_info_header.bi_compression != 0U);
        if(!s.have_strf || s.strf_size < 40U) {
            AVI_errno = AVI_ERR_NO_VIDS;
            return 0;
        }
    } else if(avi_tag_equal_ci(s.type, "auds")) {
        int t;
        if(AVI->anum >= (int)AVI_MAX_TRACKS) {
            AVI_errno = AVI_ERR_UNSUPPORTED;
            return 0;
        }
        t = AVI->anum++;
        AVI->track[t].audio_strn = (long)stream_no;
        AVI->track[t].stream_scale = s.scale;
        AVI->track[t].stream_rate = s.rate;
        AVI->track[t].stream_length = s.length;
        AVI->track[t].a_codech_off = s.strh_pos;
        if(s.have_strf) {
            AVI->track[t].a_codecf_off = s.strf_pos + 8U;
            avi_parse_wave_format(AVI, t, s.strf, s.strf_size);
        }
        if(s.sample_size && (uint64_t)s.length * s.sample_size <= 0xFFFFFFFFULL)
            AVI->track[t].audio_bytes = s.length * s.sample_size;
        if(!s.have_strf || s.strf_size < 16U) {
            AVI_errno = AVI_ERR_NO_HDRL;
            return 0;
        }
    }
    (void)have_stream;
    return 1;
}

static int avi_parse_hdrl(avi_t *AVI, uint32_t start, uint32_t end)
{
    uint32_t pos = start, data_pos, raw_end, next_pos;
    uint32_t stream_no = 0;
    uint8_t h[8], b[56];

    while(pos < end) {
        uint32_t size;
        if(!avi_get_chunk(&AVI->fdes, pos, end, h, &data_pos, &raw_end, &next_pos)) {
            AVI_errno = AVI_ERR_NO_HDRL;
            return 0;
        }
        size = avi_get_u32(h + 4);
        if(avi_tag_equal(h, "avih")) {
            if(size < 56U || !avi_read_at(&AVI->fdes, data_pos, b, 56U)) {
                AVI_errno = AVI_ERR_NO_HDRL;
                return 0;
            }
            if(AVI->fps <= 0.0) {
                uint32_t us_per_frame = avi_get_u32(b + 0);
                if(us_per_frame) AVI->fps = 1000000.0 / (double)us_per_frame;
            }
            AVI->header_video_frames = avi_get_u32(b + 16);
            if(AVI->width == 0) AVI->width = (long)avi_get_u32(b + 32);
            if(AVI->height == 0) AVI->height = (long)avi_get_u32(b + 36);
        } else if(avi_tag_equal(h, "LIST") && size >= 4U) {
            uint8_t list_type[4];
            if(!avi_read_at(&AVI->fdes, data_pos, list_type, 4U)) {
                AVI_errno = AVI_ERR_READ;
                return 0;
            }
            if(avi_tag_equal(list_type, "strl")) {
                if(!avi_parse_strl(AVI, data_pos + 4U, raw_end, stream_no)) return 0;
                stream_no++;
            }
        }
        pos = next_pos;
    }
    AVI->audio_stream_count = stream_no;
    return 1;
}

static int avi_set_stream_tags(avi_t *AVI)
{
    int i;
    if(AVI->video_strn < 0 || AVI->video_strn > 99 ||
       AVI->anum < 0 || AVI->anum > (int)AVI_MAX_TRACKS) return 0;
    if(!avi_make_stream_tag(AVI->video_strn, 'd', 'b', AVI->video_tag)) return 0;
    for(i = 0; i < (int)AVI_MAX_TRACKS; i++) {
        if(i >= AVI->anum) break;
        if(!avi_make_stream_tag(AVI->track[i].audio_strn, 'w', 'b',
                                AVI->track[i].audio_tag)) return 0;
    }
    return 1;
}

static int avi_get_idx_chunk_pos(avi_t *AVI, const avi_idx_tmp_t *idx,
                                 int base_mode, int verify, uint32_t *chunk_pos)
{
    uint64_t candidate = 0;
    uint32_t bases[5];
    int modes[5];
    uint32_t nbase = 0, i;
    uint8_t h[8];
    int selected_mode = -1;

    if(base_mode < 0) {
        bases[nbase] = idx->pos; /* Absolute file offset. */
        modes[nbase++] = 0;
        if(idx->pos <= UINT32_MAX - AVI->movi_type_pos) {
            bases[nbase] = AVI->movi_type_pos + idx->pos;
            modes[nbase++] = 1;
        }
        if(idx->pos <= UINT32_MAX - AVI->movi_start) {
            bases[nbase] = AVI->movi_start + idx->pos;
            modes[nbase++] = 2;
        }
        if(idx->pos >= 8U && idx->pos - 8U <= UINT32_MAX - AVI->movi_start) {
            bases[nbase] = AVI->movi_start + idx->pos - 8U;
            modes[nbase++] = 3;
        }
        if(idx->pos >= 8U && idx->pos - 8U <= UINT32_MAX - AVI->movi_type_pos) {
            bases[nbase] = AVI->movi_type_pos + idx->pos - 8U;
            modes[nbase++] = 4;
        }

        for(i = 0; i < nbase; i++) {
            candidate = bases[i];
            if(candidate < AVI->movi_start || candidate + 8ULL > AVI->movi_end) continue;
            if(candidate + 8ULL + avi_pad_even64(idx->len) > AVI->movi_end) continue;
            if(!avi_read_at(&AVI->fdes, (uint32_t)candidate, h, 8U)) continue;
            if(avi_tag_equal_ci(h, idx->tag) && avi_get_u32(h + 4) == idx->len) {
                selected_mode = modes[i];
                break;
            }
        }
        if(selected_mode < 0) return 0;
        *chunk_pos = (uint32_t)candidate;
        /* Translate candidate order into a stable base mode. */
        return selected_mode + 1;
    }

    switch(base_mode) {
    case 0:
        candidate = idx->pos;
        break;
    case 1:
        candidate = (uint64_t)AVI->movi_type_pos + idx->pos;
        break;
    case 2:
        candidate = (uint64_t)AVI->movi_start + idx->pos;
        break;
    case 3:
        if(idx->pos < 8U) return 0;
        candidate = (uint64_t)AVI->movi_start + idx->pos - 8U;
        break;
    case 4:
        if(idx->pos < 8U) return 0;
        candidate = (uint64_t)AVI->movi_type_pos + idx->pos - 8U;
        break;
    default:
        return 0;
    }

    if(candidate < AVI->movi_start || candidate + 8ULL > AVI->movi_end ||
       candidate + 8ULL + avi_pad_even64(idx->len) > AVI->movi_end ||
       candidate > UINT32_MAX) return 0;

    if(verify) {
        if(!avi_read_at(&AVI->fdes, (uint32_t)candidate, h, 8U)) return 0;
        if(!avi_tag_equal_ci(h, idx->tag) || avi_get_u32(h + 4) != idx->len) return 0;
    }
    *chunk_pos = (uint32_t)candidate;
    return 1;
}

static int avi_parse_idx1(avi_t *AVI)
{
    uint32_t count, i, batch_start, batch_count, k, chunk_pos;
    avi_idx_tmp_t idx;
    int base_mode = -1;
    int recognized = 0;
    int rc, is_video, audio_track;

    if(!AVI->has_idx1 || (AVI->idx1_len % 16U) != 0U) return 0;
    count = AVI->idx1_len / 16U;
    AVI->max_idx = (long)count;

    /* Read idx1 sequentially in 512-byte blocks instead of issuing one SD read per entry. */
    for(batch_start = 0; batch_start < count; batch_start += 32U) {
        batch_count = count - batch_start;
        if(batch_count > 32U) batch_count = 32U;
        if(!avi_read_at(&AVI->fdes, AVI->idx1_pos + batch_start * 16U,
                        g_avi_idx_buffer, batch_count * 16U)) return 0;

        for(k = 0; k < batch_count; k++) {
            i = batch_start + k;
            memcpy(idx.tag, g_avi_idx_buffer + k * 16U, 4U);
            idx.flags = avi_get_u32(g_avi_idx_buffer + k * 16U + 4U);
            idx.pos = avi_get_u32(g_avi_idx_buffer + k * 16U + 8U);
            idx.len = avi_get_u32(g_avi_idx_buffer + k * 16U + 12U);

            is_video = avi_is_video_chunk(AVI, idx.tag);
            audio_track = -1;
            if(!is_video) {
                audio_track = avi_audio_track_from_tag(AVI, idx.tag);
                if(audio_track < 0) continue;
            }

            /* Verify the first, last, and periodic entries; all others use the
               already established offset convention plus strict movi bounds. */
            rc = avi_get_idx_chunk_pos(AVI, &idx, base_mode,
                                       (base_mode < 0 || i == count - 1U || (i & 63U) == 0U),
                                       &chunk_pos);
            if(rc == 0) return 0;
            if(base_mode < 0) base_mode = rc - 1;

            if(is_video) {
                if(!avi_add_video_index(AVI, chunk_pos + 8U, idx.len,
                                        idx.flags & AVIIF_KEYFRAME, idx.tag)) return -1;
            } else {
                if(!avi_add_audio_index(AVI, audio_track, chunk_pos + 8U, idx.len)) return -1;
            }
            recognized++;
        }
    }

    AVI->n_idx = recognized;
    if(AVI->video_chunk_count == 0U) return 0;
    if(AVI->header_video_frames && AVI->video_chunk_count < AVI->header_video_frames)
        return 0;
    return 1;
}

static int avi_scan_movi_region(avi_t *AVI, uint32_t start, uint32_t end,
                                unsigned int depth)
{
    uint32_t pos = start, data_pos, raw_end, next_pos, size;
    uint8_t h[8], list_type[4];

    if(depth > AVI_MAX_RECURSION) {
        AVI_errno = AVI_ERR_UNSUPPORTED;
        return 0;
    }

    while(pos < end) {
        int audio_track;
        if(end - pos < 8U) break;
        if(!avi_get_chunk(&AVI->fdes, pos, end, h, &data_pos, &raw_end, &next_pos)) {
            AVI_errno = AVI_ERR_NO_MOVI;
            return 0;
        }
        size = avi_get_u32(h + 4);
        if(avi_tag_equal(h, "LIST")) {
            if(size < 4U || !avi_read_at(&AVI->fdes, data_pos, list_type, 4U)) {
                AVI_errno = AVI_ERR_NO_MOVI;
                return 0;
            }
            if(avi_tag_equal(list_type, "rec ")) {
                if(!avi_scan_movi_region(AVI, data_pos + 4U, raw_end, depth + 1U)) return 0;
            }
        } else if(avi_is_video_chunk(AVI, (char *)h)) {
            uint32_t flags = 0;
            /* MJPEG and uncompressed frames are independently decodable. */
            if(avi_tag_equal_ci(AVI->compressor, "MJPG") ||
               avi_tag_equal_ci(AVI->compressor2, "MJPG") ||
               AVI->bitmap_info_header.bi_compression == 0U)
                flags = AVIIF_KEYFRAME;
            if(!avi_add_video_index(AVI, data_pos, size, flags, (char *)h)) return 0;
            AVI->n_idx++;
        } else {
            audio_track = avi_audio_track_from_tag(AVI, (char *)h);
            if(audio_track >= 0) {
                if(!avi_add_audio_index(AVI, audio_track, data_pos, size)) return 0;
                AVI->n_idx++;
            }
        }
        pos = next_pos;
    }
    return 1;
}

static int avi_build_indexes(avi_t *AVI)
{
    int idx_result = 0;
    avi_reset_indexes(AVI);

    if(AVI->has_idx1) {
        idx_result = avi_parse_idx1(AVI);
        if(idx_result < 0) return 0; /* Index capacity exceeded. */
    }

    if(idx_result == 0) {
        avi_reset_indexes(AVI);
        if(!avi_scan_movi_region(AVI, AVI->movi_start, AVI->movi_end, 0U)) return 0;
    }

    if(AVI->video_chunk_count == 0U) {
        AVI_errno = AVI_ERR_NO_IDX;
        return 0;
    }
    AVI->video_frames = (long)AVI->video_chunk_count;
    AVI->index_ready = 1;
    AVI->video_pos = 0;
    AVI->aptr = 0;
    {
        int i;
        for(i = 0; i < AVI->anum; i++) {
            AVI->track[i].audio_posc = 0;
            AVI->track[i].audio_posb = 0;
        }
    }
    return 1;
}

static int avi_parse_input_file(avi_t *AVI, int getIndex)
{
    uint8_t h[12], chunk[8], list_type[4];
    uint32_t riff_size, pos, data_pos, raw_end, next_pos, size;
    uint64_t riff_end64;

    if(!avi_read_at(&AVI->fdes, 0U, h, 12U)) {
        AVI_errno = AVI_ERR_READ;
        return 0;
    }
    if(!avi_tag_equal((char *)h, "RIFF") || !avi_tag_equal((char *)h + 8, "AVI ")) {
        AVI_errno = AVI_ERR_NO_AVI;
        return 0;
    }

    riff_size = avi_get_u32(h + 4);
    riff_end64 = 8ULL + riff_size;
    if(riff_end64 < 12ULL || riff_end64 > AVI->file_size) {
        AVI_errno = AVI_ERR_NO_AVI;
        return 0;
    }
    AVI->riff_end = (uint32_t)riff_end64;
    if(AVI->riff_end + 12U <= AVI->file_size) {
        uint8_t tail[12];
        if(avi_read_at(&AVI->fdes, AVI->riff_end, tail, 12U) &&
           avi_tag_equal(tail, "RIFF") && avi_tag_equal(tail + 8, "AVIX")) {
            AVI_errno = AVI_ERR_UNSUPPORTED;
            return 0;
        }
    }

    pos = 12U;
    while(pos + 8U <= AVI->riff_end) {
        if(!avi_get_chunk(&AVI->fdes, pos, AVI->riff_end, chunk,
                          &data_pos, &raw_end, &next_pos)) {
            AVI_errno = AVI_ERR_NO_AVI;
            return 0;
        }
        size = avi_get_u32(chunk + 4);
        if(avi_tag_equal(chunk, "LIST")) {
            if(size < 4U || !avi_read_at(&AVI->fdes, data_pos, list_type, 4U)) {
                AVI_errno = AVI_ERR_NO_AVI;
                return 0;
            }
            if(avi_tag_equal(list_type, "hdrl")) {
                if(!avi_parse_hdrl(AVI, data_pos + 4U, raw_end)) return 0;
            } else if(avi_tag_equal(list_type, "movi")) {
                if(AVI->movi_start == 0U) {
                    AVI->movi_type_pos = data_pos;
                    AVI->movi_start = data_pos + 4U;
                    AVI->movi_end = raw_end;
                }
            }
        } else if(avi_tag_equal(chunk, "idx1")) {
            if(!AVI->has_idx1) {
                AVI->has_idx1 = 1;
                AVI->idx1_pos = data_pos;
                AVI->idx1_len = size;
            }
        } else if(avi_tag_equal(chunk, "RIFF")) {
            /* OpenDML AVIX segments are outside this AVI 1.0 reader. */
            AVI_errno = AVI_ERR_UNSUPPORTED;
            return 0;
        }
        pos = next_pos;
    }

    if(AVI->video_strn < 0) {
        AVI_errno = AVI_ERR_NO_VIDS;
        return 0;
    }
    if(AVI->movi_start == 0U || AVI->movi_end <= AVI->movi_start) {
        AVI_errno = AVI_ERR_NO_MOVI;
        return 0;
    }
    if(!avi_set_stream_tags(AVI)) {
        AVI_errno = AVI_ERR_UNSUPPORTED;
        return 0;
    }
    if(AVI->fps <= 0.0 && AVI->video_stream_scale && AVI->video_stream_rate)
        AVI->fps = (double)AVI->video_stream_rate / (double)AVI->video_stream_scale;

    if(!getIndex) {
        AVI->video_frames = AVI->header_video_frames;
        AVI->index_ready = 0;
        return 1;
    }

    if(!avi_build_indexes(AVI)) {
        if(AVI_errno == AVI_ERR_NONE) AVI_errno = AVI_ERR_NO_IDX;
        return 0;
    }
    return 1;
}

avi_t *AVI_open_input_file(const char *filename, int getIndex)
{
    avi_t *AVI = &g_avi_instance;
    FSIZE_t fsz;
    FRESULT fr;

    AVI_errno = AVI_ERR_NONE;
    if(filename == NULL) {
        AVI_errno = AVI_ERR_OPEN;
        return NULL;
    }
    memset(AVI, 0, sizeof(*AVI));
    AVI->mode = AVI_MODE_READ;
    AVI->video_strn = -1;
    AVI->aptr = 0;

    fr = f_open(&AVI->fdes, filename, FA_READ);
    if(fr != FR_OK) {
        AVI_errno = AVI_ERR_OPEN;
        return NULL;
    }
    AVI->is_open = 1;
    fsz = f_size(&AVI->fdes);
    if((uint64_t)fsz > 0xFFFFFFFFULL) {
        AVI_errno = AVI_ERR_SIZELIM;
        f_close(&AVI->fdes);
        AVI->is_open = 0;
        return NULL;
    }
    AVI->file_size = (uint32_t)fsz;

    if(!avi_parse_input_file(AVI, getIndex)) {
        if(AVI_errno == AVI_ERR_NONE) AVI_errno = AVI_ERR_NO_AVI;
        f_close(&AVI->fdes);
        AVI->is_open = 0;
        return NULL;
    }
    return AVI;
}

/* ------------------------------- Writer -------------------------------- */

static uint32_t avi_audio_avg_bytes(const track_t *t)
{
    if(t->avg_bytes_per_sec) return t->avg_bytes_per_sec;
    if(t->a_fmt == 1 && t->a_rate > 0 && t->block_align > 0)
        return (uint32_t)t->a_rate * (uint32_t)t->block_align;
    if(t->mp3rate > 0) return (uint32_t)t->mp3rate * 1000U / 8U;
    return 0U;
}

static int avi_header_out4(uint32_t *p, const char *s)
{
    if(*p > HEADERBYTES - 4U) return 0;
    memcpy(g_avi_header_buf + *p, s, 4U);
    *p += 4U;
    return 1;
}

static int avi_header_out32(uint32_t *p, uint32_t v)
{
    if(*p > HEADERBYTES - 4U) return 0;
    avi_put_u32(g_avi_header_buf + *p, v);
    *p += 4U;
    return 1;
}

static int avi_header_out16(uint32_t *p, uint16_t v)
{
    if(*p > HEADERBYTES - 2U) return 0;
    avi_put_u16(g_avi_header_buf + *p, v);
    *p += 2U;
    return 1;
}

static int avi_build_output_header(avi_t *AVI, uint32_t file_end)
{
    uint32_t p = 0, hdrl_size_pos, strl_size_pos, movi_header_pos;
    uint32_t us_per_frame, fps_scale = AVI_FRAME_RATE_SCALE;
    uint32_t rate, i, k, remaining, junk_size, max_audio_chunk;
    uint32_t movi_size = (AVI->movi_end - AVI->movi_start) + 4U;
    uint32_t block_align, avg_bytes;
    uint64_t image_size;
    char zero_fourcc[4] = {0, 0, 0, 0};

    memset(g_avi_header_buf, 0, sizeof(g_avi_header_buf));
    if(AVI->fps > 0.000001) {
        us_per_frame = (uint32_t)(1000000.0 / AVI->fps + 0.5);
        rate = (uint32_t)(AVI->fps * (double)fps_scale + 0.5);
        if(rate == 0U) rate = 1U;
    } else {
        us_per_frame = 0U;
        rate = 0U;
    }

    if(!avi_header_out4(&p, "RIFF") || !avi_header_out32(&p, file_end - 8U) ||
       !avi_header_out4(&p, "AVI ") || !avi_header_out4(&p, "LIST")) return 0;
    hdrl_size_pos = p;
    if(!avi_header_out32(&p, 0U) || !avi_header_out4(&p, "hdrl")) return 0;

    if(!avi_header_out4(&p, "avih") || !avi_header_out32(&p, 56U) ||
       !avi_header_out32(&p, us_per_frame) || !avi_header_out32(&p, 0U) ||
       !avi_header_out32(&p, 0U) ||
       !avi_header_out32(&p, AVIF_HASINDEX | (AVI->anum ? AVIF_ISINTERLEAVED : 0U)) ||
       !avi_header_out32(&p, (uint32_t)AVI->video_frames) ||
       !avi_header_out32(&p, 0U) || !avi_header_out32(&p, (uint32_t)(AVI->anum + 1)) ||
       !avi_header_out32(&p, AVI->max_len) || !avi_header_out32(&p, (uint32_t)AVI->width) ||
       !avi_header_out32(&p, (uint32_t)AVI->height)) return 0;
    for(i = 0; i < 4U; i++) if(!avi_header_out32(&p, 0U)) return 0;

    /* Video stream list. */
    if(!avi_header_out4(&p, "LIST")) return 0;
    strl_size_pos = p;
    if(!avi_header_out32(&p, 0U) || !avi_header_out4(&p, "strl") ||
       !avi_header_out4(&p, "strh") || !avi_header_out32(&p, 56U) ||
       !avi_header_out4(&p, "vids")) return 0;
    if(AVI->compressor[0]) {
        if(!avi_header_out4(&p, AVI->compressor)) return 0;
    } else if(!avi_header_out4(&p, zero_fourcc)) return 0;
    if(!avi_header_out32(&p, 0U) || !avi_header_out16(&p, 0U) ||
       !avi_header_out16(&p, 0U) || !avi_header_out32(&p, 0U) ||
       !avi_header_out32(&p, fps_scale) || !avi_header_out32(&p, rate) ||
       !avi_header_out32(&p, 0U) || !avi_header_out32(&p, (uint32_t)AVI->video_frames) ||
       !avi_header_out32(&p, AVI->max_len) || !avi_header_out32(&p, 0xFFFFFFFFUL) ||
       !avi_header_out32(&p, 0U)) return 0;
    for(i = 0; i < 4U; i++) if(!avi_header_out16(&p, 0U)) return 0;

    if(!avi_header_out4(&p, "strf") || !avi_header_out32(&p, 40U) ||
       !avi_header_out32(&p, 40U) || !avi_header_out32(&p, (uint32_t)AVI->width) ||
       !avi_header_out32(&p, (uint32_t)AVI->height) || !avi_header_out16(&p, 1U) ||
       !avi_header_out16(&p, 24U)) return 0;
    if(AVI->compressor[0]) {
        if(!avi_header_out4(&p, AVI->compressor)) return 0;
    } else if(!avi_header_out32(&p, 0U)) return 0;
    image_size = (uint64_t)(uint32_t)AVI->width * (uint32_t)AVI->height * 3ULL;
    if(image_size > 0xFFFFFFFFULL) image_size = 0U;
    if(!avi_header_out32(&p, (uint32_t)image_size) ||
       !avi_header_out32(&p, 0U) || !avi_header_out32(&p, 0U) ||
       !avi_header_out32(&p, 0U) || !avi_header_out32(&p, 0U)) return 0;
    avi_put_u32(g_avi_header_buf + strl_size_pos, p - (strl_size_pos + 4U));

    /* Audio stream lists. */
    for(i = 0; i < (uint32_t)AVI->anum; i++) {
        track_t *t = &AVI->track[i];
        block_align = (t->a_fmt == 1) ? (uint32_t)t->a_chans * ((uint32_t)t->a_bits + 7U) / 8U : 1U;
        if(block_align == 0U || block_align > 0xFFFFU) block_align = 1U;
        avg_bytes = avi_audio_avg_bytes(t);
        max_audio_chunk = t->max_chunk;

        if(!avi_header_out4(&p, "LIST")) return 0;
        strl_size_pos = p;
        if(!avi_header_out32(&p, 0U) || !avi_header_out4(&p, "strl") ||
           !avi_header_out4(&p, "strh") || !avi_header_out32(&p, 56U) ||
           !avi_header_out4(&p, "auds") || !avi_header_out32(&p, 0U) ||
           !avi_header_out32(&p, 0U) || !avi_header_out16(&p, 0U) ||
           !avi_header_out16(&p, 0U) || !avi_header_out32(&p, 0U) ||
           !avi_header_out32(&p, block_align) || !avi_header_out32(&p, avg_bytes) ||
           !avi_header_out32(&p, 0U) ||
           !avi_header_out32(&p, (block_align ? t->audio_bytes / block_align : t->audio_bytes)) ||
           !avi_header_out32(&p, max_audio_chunk) || !avi_header_out32(&p, 0xFFFFFFFFUL) ||
           !avi_header_out32(&p, block_align)) return 0;
        for(k = 0; k < 4U; k++) if(!avi_header_out16(&p, 0U)) return 0;

        if(!avi_header_out4(&p, "strf") || !avi_header_out32(&p, 16U) ||
           !avi_header_out16(&p, (uint16_t)t->a_fmt) ||
           !avi_header_out16(&p, (uint16_t)t->a_chans) ||
           !avi_header_out32(&p, (uint32_t)t->a_rate) ||
           !avi_header_out32(&p, avg_bytes) ||
           !avi_header_out16(&p, (uint16_t)block_align) ||
           !avi_header_out16(&p, (uint16_t)t->a_bits)) return 0;
        avi_put_u32(g_avi_header_buf + strl_size_pos, p - (strl_size_pos + 4U));
    }

    avi_put_u32(g_avi_header_buf + hdrl_size_pos, p - (hdrl_size_pos + 4U));

    /* Reserve the rest of the 2 KiB header using a JUNK chunk. */
    movi_header_pos = HEADERBYTES - 12U;
    if(p + 8U > movi_header_pos) return 0;
    remaining = movi_header_pos - p;
    if(remaining >= 8U) {
        junk_size = remaining - 8U;
        if(!avi_header_out4(&p, "JUNK") || !avi_header_out32(&p, junk_size)) return 0;
        if(p + junk_size > HEADERBYTES) return 0;
        memset(g_avi_header_buf + p, 0, junk_size);
        p += junk_size;
    }
    if(p != movi_header_pos) return 0;
    if(!avi_header_out4(&p, "LIST") || !avi_header_out32(&p, movi_size) ||
       !avi_header_out4(&p, "movi")) return 0;
    return (p == HEADERBYTES);
}

static int avi_write_chunk(avi_t *AVI, const char tag[4], const char *data,
                           uint32_t len, uint32_t *data_pos)
{
    uint8_t h[8];
    uint8_t pad = 0;
    uint64_t next;

    next = (uint64_t)AVI->pos + 8ULL + avi_pad_even64(len);
    if(next > AVI_MAX_FILE_SIZE) {
        AVI_errno = AVI_ERR_SIZELIM;
        return 0;
    }
    memcpy(h, tag, 4U);
    avi_put_u32(h + 4, len);
    if(!avi_seek(&AVI->fdes, AVI->pos) || !avi_write_exact(&AVI->fdes, h, 8U) ||
       !avi_write_exact(&AVI->fdes, data, len)) {
        AVI_errno = AVI_ERR_WRITE;
        return 0;
    }
    if(len & 1U) {
        if(!avi_write_exact(&AVI->fdes, &pad, 1U)) {
            AVI_errno = AVI_ERR_WRITE;
            return 0;
        }
    }
    if(data_pos) *data_pos = AVI->pos + 8U;
    AVI->pos = (uint32_t)next;
    return 1;
}

avi_t *AVI_open_output_file(char *filename)
{
    avi_t *AVI = &g_avi_instance;
    FRESULT fr;

    AVI_errno = AVI_ERR_NONE;
    if(filename == NULL) {
        AVI_errno = AVI_ERR_OPEN;
        return NULL;
    }
    memset(AVI, 0, sizeof(*AVI));
    AVI->mode = AVI_MODE_WRITE;
    AVI->video_strn = 0;
    memcpy(AVI->video_tag, "00db", 4U);
    AVI->pos = HEADERBYTES;
    AVI->movi_start = HEADERBYTES;
    AVI->movi_type_pos = HEADERBYTES - 4U;

    fr = f_open(&AVI->fdes, filename, FA_CREATE_ALWAYS | FA_READ | FA_WRITE);
    if(fr != FR_OK) {
        AVI_errno = AVI_ERR_OPEN;
        return NULL;
    }
    AVI->is_open = 1;
    memset(g_avi_header_buf, 0, sizeof(g_avi_header_buf));
    if(!avi_write_exact(&AVI->fdes, g_avi_header_buf, HEADERBYTES)) {
        AVI_errno = AVI_ERR_WRITE;
        f_close(&AVI->fdes);
        AVI->is_open = 0;
        return NULL;
    }
    return AVI;
}

void AVI_set_video(avi_t *AVI, int width, int height, double fps, char *compressor)
{
    if(!AVI || AVI->mode != AVI_MODE_WRITE || width <= 0 || height <= 0) return;
    AVI->width = width;
    AVI->height = height;
    AVI->fps = fps;
    memset(AVI->compressor, 0, sizeof(AVI->compressor));
    if(compressor && strncmp(compressor, "RGB", 3) != 0) {
        size_t n = strlen(compressor);
        if(n > 4U) n = 4U;
        memcpy(AVI->compressor, compressor, n);
    }
    if(AVI->compressor[0]) memcpy(AVI->compressor2, AVI->compressor, 4U);
    else memset(AVI->compressor2, 0, 4U);
    AVI->compressor[4] = 0;
    AVI->compressor2[4] = 0;
    AVI->bitmap_info_header.bi_size = 40U;
    AVI->bitmap_info_header.bi_width = width;
    AVI->bitmap_info_header.bi_height = height;
    AVI->bitmap_info_header.bi_planes = 1U;
    AVI->bitmap_info_header.bi_bit_count = 24U;
    AVI->bitmap_info_header.bi_compression = AVI->compressor[0] ?
        ((uint32_t)(uint8_t)AVI->compressor[0] |
         ((uint32_t)(uint8_t)AVI->compressor[1] << 8) |
         ((uint32_t)(uint8_t)AVI->compressor[2] << 16) |
         ((uint32_t)(uint8_t)AVI->compressor[3] << 24)) : 0U;
    AVI->video_is_compressed = (AVI->compressor[0] != 0);
}

void AVI_set_audio(avi_t *AVI, int channels, long rate, int bits, int format, long mp3rate)
{
    track_t *t;
    int index;
    if(!AVI || AVI->mode != AVI_MODE_WRITE || AVI->anum >= (int)AVI_MAX_TRACKS ||
       channels <= 0 || rate <= 0 || bits < 0) return;
    index = AVI->anum++;
    AVI->aptr = index;
    t = &AVI->track[index];
    memset(t, 0, sizeof(*t));
    t->a_chans = channels;
    t->a_rate = rate;
    t->a_bits = bits;
    t->a_fmt = format;
    t->mp3rate = mp3rate;
    t->audio_strn = index + 1;
    t->block_align = (format == 1) ? (uint16_t)(channels * ((bits + 7) / 8)) : 1U;
    if(t->block_align == 0U) t->block_align = 1U;
    t->avg_bytes_per_sec = (format == 1) ?
        (uint32_t)rate * t->block_align : (uint32_t)((mp3rate > 0) ? mp3rate * 1000L / 8L : 0L);
    t->cb_size = 0;
    avi_make_stream_tag(t->audio_strn, 'w', 'b', t->audio_tag);
}

int AVI_write_frame(avi_t *AVI, char *data, long bytes, int keyframe)
{
    uint32_t pos;
    const char *tag;
    if(!AVI || AVI->mode != AVI_MODE_WRITE || !data || bytes <= 0 ||
       AVI->video_chunk_count >= MAX_AVI_FRAMES || AVI->width <= 0 || AVI->height <= 0)
        return -1;
    tag = AVI->video_is_compressed ? "00dc" : "00db";
    if(!avi_write_chunk(AVI, tag, data, (uint32_t)bytes, &pos)) return -1;
    if(!avi_add_video_index(AVI, pos, (uint32_t)bytes,
                            keyframe ? AVIIF_KEYFRAME : 0U, tag)) return -1;
    AVI->last_pos = pos;
    AVI->last_len = (uint32_t)bytes;
    AVI->video_frames = (long)AVI->video_chunk_count;
    return 0;
}

int AVI_write_audio(avi_t *AVI, char *data, long bytes)
{
    uint32_t pos;
    track_t *t;
    if(!AVI || AVI->mode != AVI_MODE_WRITE || !data || bytes <= 0 || AVI->anum <= 0 ||
       AVI->aptr < 0 || AVI->aptr >= AVI->anum) return -1;
    t = &AVI->track[AVI->aptr];
    if(t->audio_chunks >= (long)MAX_AVI_AUDIO_CHUNKS) {
        AVI_errno = AVI_ERR_INDEX_LIMIT;
        return -1;
    }
    if(!avi_write_chunk(AVI, t->audio_tag, data, (uint32_t)bytes, &pos)) return -1;
    if(!avi_add_audio_index(AVI, AVI->aptr, pos, (uint32_t)bytes)) return -1;
    AVI->n_idx++;
    return 0;
}

int AVI_append_audio(avi_t *AVI, char *data, long bytes)
{
    track_t *t;
    audio_index_entry *e;
    uint32_t old_len, new_len, new_pad, data_end, header_pos;
    uint8_t sz[4], pad = 0;
    if(!AVI || AVI->mode != AVI_MODE_WRITE || !data || bytes <= 0 || AVI->anum <= 0 ||
       AVI->aptr < 0 || AVI->aptr >= AVI->anum) return -1;
    t = &AVI->track[AVI->aptr];
    if(t->audio_chunks <= 0) return -1;
    e = &t->audio_index[t->audio_chunks - 1];
    old_len = e->len;
    if((uint64_t)old_len + (uint32_t)bytes > 0xFFFFFFFEULL) return -1;
    new_len = old_len + (uint32_t)bytes;
    header_pos = e->pos - 8U;
    data_end = e->pos + old_len;
    if(data_end + (old_len & 1U) != AVI->pos) return -1; /* Must be the last written chunk. */
    if(!avi_seek(&AVI->fdes, data_end) ||
       !avi_write_exact(&AVI->fdes, data, (uint32_t)bytes)) return -1;
    new_pad = new_len & 1U;
    if(new_pad && !avi_write_exact(&AVI->fdes, &pad, 1U)) return -1;
    avi_put_u32(sz, new_len);
    if(!avi_seek(&AVI->fdes, header_pos + 4U) || !avi_write_exact(&AVI->fdes, sz, 4U)) return -1;
    AVI->pos = (uint32_t)(e->pos + avi_pad_even64(new_len));
    e->len = new_len;
    t->audio_bytes += (uint32_t)bytes;
    if(new_len > t->max_chunk) t->max_chunk = new_len;
    return 0;
}

int AVI_dup_frame(avi_t *AVI)
{
    uint32_t n;
    if(!AVI || AVI->mode != AVI_MODE_WRITE || AVI->last_pos == 0U) return -1;
    n = AVI->video_chunk_count;
    if(n >= MAX_AVI_FRAMES) {
        AVI_errno = AVI_ERR_INDEX_LIMIT;
        return -1;
    }
    AVI->video_index[n].pos = AVI->last_pos;
    AVI->video_index[n].len = AVI->last_len;
    AVI->video_index[n].key = (AVI->video_chunk_count > 0U) ?
        AVI->video_index[AVI->video_chunk_count - 1U].key : AVIIF_KEYFRAME;
    AVI->video_chunk_count++;
    AVI->video_frames = (long)AVI->video_chunk_count;
    return 0;
}

static int avi_write_idx1(avi_t *AVI)
{
    uint32_t total = AVI->video_chunk_count;
    uint32_t vi = 0, ai[AVI_MAX_TRACKS] = {0, 0};
    uint32_t i;
    uint8_t h[8], e[16];
    uint64_t idx_size;
    int t;

    for(i = 0; i < (uint32_t)AVI->anum; i++) total += (uint32_t)AVI->track[i].audio_chunks;
    idx_size = (uint64_t)total * 16ULL;
    if(idx_size > 0xFFFFFFFFULL || (uint64_t)AVI->pos + 8ULL + idx_size > AVI_MAX_FILE_SIZE) {
        AVI_errno = AVI_ERR_SIZELIM;
        return 0;
    }
    memcpy(h, "idx1", 4U);
    avi_put_u32(h + 4, (uint32_t)idx_size);
    if(!avi_seek(&AVI->fdes, AVI->pos) || !avi_write_exact(&AVI->fdes, h, 8U)) {
        AVI_errno = AVI_ERR_WRITE_INDEX;
        return 0;
    }

    for(i = 0; i < total; i++) {
        uint32_t best_pos = 0xFFFFFFFFUL;
        int best_type = -1;
        int best_track = -1;
        uint32_t hdr_pos;

        if(vi < AVI->video_chunk_count) {
            hdr_pos = AVI->video_index[vi].pos - 8U;
            if(hdr_pos < best_pos) {
                best_pos = hdr_pos;
                best_type = 0;
            }
        }
        for(t = 0; t < AVI->anum; t++) {
            if(ai[t] < (uint32_t)AVI->track[t].audio_chunks) {
                hdr_pos = AVI->track[t].audio_index[ai[t]].pos - 8U;
                if(hdr_pos < best_pos) {
                    best_pos = hdr_pos;
                    best_type = 1;
                    best_track = t;
                }
            }
        }
        if(best_type < 0 || best_pos < AVI->movi_type_pos) {
            AVI_errno = AVI_ERR_WRITE_INDEX;
            return 0;
        }
        memset(e, 0, sizeof(e));
        if(best_type == 0) {
            memcpy(e, AVI->video_tag, 4U);
            avi_put_u32(e + 4, AVI->video_index[vi].key);
            avi_put_u32(e + 8, best_pos - AVI->movi_type_pos);
            avi_put_u32(e + 12, AVI->video_index[vi].len);
            vi++;
        } else {
            track_t *audio = &AVI->track[best_track];
            memcpy(e, audio->audio_tag, 4U);
            avi_put_u32(e + 4, 0U);
            avi_put_u32(e + 8, best_pos - AVI->movi_type_pos);
            avi_put_u32(e + 12, audio->audio_index[ai[best_track]].len);
            ai[best_track]++;
        }
        if(!avi_write_exact(&AVI->fdes, e, 16U)) {
            AVI_errno = AVI_ERR_WRITE_INDEX;
            return 0;
        }
    }
    AVI->pos += 8U + (uint32_t)idx_size;
    AVI->n_idx = (long)total;
    return 1;
}

static int avi_close_output_file(avi_t *AVI)
{
    uint32_t file_end;
    if(!avi_write_idx1(AVI)) return -1;
    AVI->movi_end = AVI->pos;
    file_end = AVI->pos;
    if(!avi_build_output_header(AVI, file_end)) {
        AVI_errno = AVI_ERR_CLOSE;
        return -1;
    }
    if(!avi_seek(&AVI->fdes, 0U) ||
       !avi_write_exact(&AVI->fdes, g_avi_header_buf, HEADERBYTES) ||
       !avi_seek(&AVI->fdes, file_end) || f_sync(&AVI->fdes) != FR_OK) {
        AVI_errno = AVI_ERR_CLOSE;
        return -1;
    }
    return 0;
}

/* ------------------------------ Public API ----------------------------- */

int AVI_close(avi_t *AVI)
{
    int ret = 0;
    if(!AVI || !AVI->is_open) return 0;
    if(AVI->mode == AVI_MODE_WRITE) ret = avi_close_output_file(AVI);
    if(f_close(&AVI->fdes) != FR_OK && ret == 0) {
        AVI_errno = AVI_ERR_CLOSE;
        ret = -1;
    }
    AVI->is_open = 0;
    return ret;
}

long AVI_video_frames(avi_t *AVI) { return AVI ? AVI->video_frames : 0; }
int AVI_video_width(avi_t *AVI) { return AVI ? (int)AVI->width : 0; }
int AVI_video_height(avi_t *AVI) { return AVI ? (int)AVI->height : 0; }
double AVI_frame_rate(avi_t *AVI) { return AVI ? AVI->fps : 0.0; }
char *AVI_video_compressor(avi_t *AVI) { return AVI ? AVI->compressor2 : NULL; }
long AVI_max_video_chunk(avi_t *AVI) { return AVI ? (long)AVI->max_len : 0; }
int AVI_audio_tracks(avi_t *AVI) { return AVI ? AVI->anum : 0; }
int AVI_audio_channels(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? (int)AVI->track[AVI->aptr].a_chans : 0; }
int AVI_audio_bits(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? (int)AVI->track[AVI->aptr].a_bits : 0; }
int AVI_audio_format(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? (int)AVI->track[AVI->aptr].a_fmt : 0; }
long AVI_audio_rate(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? AVI->track[AVI->aptr].a_rate : 0; }
long AVI_audio_bytes(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? (long)AVI->track[AVI->aptr].audio_bytes : 0; }
long AVI_audio_chunks(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? AVI->track[AVI->aptr].audio_chunks : 0; }
long AVI_audio_mp3rate(avi_t *AVI) { return (AVI && AVI->aptr >= 0 && AVI->aptr < AVI->anum) ? AVI->track[AVI->aptr].mp3rate : 0; }
int AVI_set_audio_track(avi_t *AVI, int track) { if(!AVI || track < 0 || track >= AVI->anum) return -1; AVI->aptr = track; return 0; }
int AVI_get_audio_track(avi_t *AVI) { return AVI ? AVI->aptr : -1; }
long AVI_get_error(void) { return AVI_errno; }

long AVI_frame_size(avi_t *AVI, long frame)
{
    if(!AVI || AVI->mode != AVI_MODE_READ || !AVI->index_ready ||
       frame < 0 || (uint32_t)frame >= AVI->video_chunk_count) return 0;
    return (long)AVI->video_index[frame].len;
}

int AVI_seek_start(avi_t *AVI)
{
    int i;
    if(!AVI || AVI->mode != AVI_MODE_READ || !AVI->index_ready) return -1;
    if(!avi_seek(&AVI->fdes, AVI->movi_start)) return -1;
    AVI->video_pos = 0;
    for(i = 0; i < AVI->anum; i++) {
        AVI->track[i].audio_posc = 0;
        AVI->track[i].audio_posb = 0;
    }
    return 0;
}

int AVI_set_video_position(avi_t *AVI, long frame)
{
    if(!AVI || AVI->mode != AVI_MODE_READ || !AVI->index_ready) return -1;
    if(frame < 0) frame = 0;
    if((uint32_t)frame > AVI->video_chunk_count) frame = (long)AVI->video_chunk_count;
    AVI->video_pos = frame;
    return 0;
}

long AVI_read_frame(avi_t *AVI, char *vidbuf, int *keyframe)
{
    uint32_t len;
    if(!AVI || AVI->mode != AVI_MODE_READ || !AVI->index_ready || !vidbuf ||
       AVI->video_pos < 0 || (uint32_t)AVI->video_pos >= AVI->video_chunk_count)
        return -1;
    len = AVI->video_index[AVI->video_pos].len;
    if(keyframe) *keyframe = ((AVI->video_index[AVI->video_pos].key & AVIIF_KEYFRAME) != 0U);
    if(!avi_seek(&AVI->fdes, AVI->video_index[AVI->video_pos].pos) ||
       !avi_read_exact(&AVI->fdes, vidbuf, len)) {
        AVI_errno = AVI_ERR_READ;
        return -1;
    }
    AVI->video_pos++;
    return (long)len;
}

long AVI_read_frame_ex(avi_t *AVI, char *vidbuf, uint32_t capacity, int *keyframe)
{
    uint32_t need;
    if(!AVI || AVI->mode != AVI_MODE_READ || !AVI->index_ready ||
       AVI->video_pos < 0 || (uint32_t)AVI->video_pos >= AVI->video_chunk_count)
        return -1;
    need = AVI->video_index[AVI->video_pos].len;
    if(capacity < need) {
        AVI_errno = AVI_ERR_SIZELIM;
        return -1;
    }
    return AVI_read_frame(AVI, vidbuf, keyframe);
}

int AVI_set_audio_position(avi_t *AVI, long byte)
{
    track_t *t;
    uint32_t lo, hi, mid, target;
    if(!AVI || AVI->mode != AVI_MODE_READ || AVI->aptr < 0 || AVI->aptr >= AVI->anum)
        return -1;
    t = &AVI->track[AVI->aptr];
    if(byte < 0) byte = 0;
    if((uint64_t)byte > t->audio_bytes) byte = (long)t->audio_bytes;
    target = (uint32_t)byte;
    if(t->audio_chunks <= 0 || target >= t->audio_bytes) {
        t->audio_posc = t->audio_chunks;
        t->audio_posb = 0;
        return 0;
    }

    lo = 0U;
    hi = (uint32_t)t->audio_chunks;
    while(lo < hi) {
        mid = lo + (hi - lo) / 2U;
        if(t->audio_index[mid].tot <= target) lo = mid + 1U;
        else hi = mid;
    }
    if(lo == 0U) lo = 1U;
    t->audio_posc = (long)(lo - 1U);
    t->audio_posb = (long)(target - t->audio_index[lo - 1U].tot);
    return 0;
}

long AVI_read_audio(avi_t *AVI, char *audbuf, long bytes)
{
    track_t *t;
    long nr = 0;
    if(!AVI || AVI->mode != AVI_MODE_READ || !audbuf || bytes < 0 ||
       AVI->aptr < 0 || AVI->aptr >= AVI->anum) return -1;
    if(bytes == 0) return 0;
    t = &AVI->track[AVI->aptr];

    while(bytes > 0 && t->audio_posc < t->audio_chunks) {
        audio_index_entry *e = &t->audio_index[t->audio_posc];
        uint32_t left, todo;
        if(t->audio_posb < 0 || (uint32_t)t->audio_posb > e->len) {
            AVI_errno = AVI_ERR_READ;
            return -1;
        }
        left = e->len - (uint32_t)t->audio_posb;
        if(left == 0U) {
            t->audio_posc++;
            t->audio_posb = 0;
            continue;
        }
        todo = ((uint32_t)bytes < left) ? (uint32_t)bytes : left;
        if(!avi_seek(&AVI->fdes, e->pos + (uint32_t)t->audio_posb) ||
           !avi_read_exact(&AVI->fdes, audbuf + nr, todo)) {
            AVI_errno = AVI_ERR_READ;
            return -1;
        }
        nr += (long)todo;
        bytes -= (long)todo;
        t->audio_posb += (long)todo;
    }
    return nr;
}

long AVI_read_audio_chunk(avi_t *AVI, char *audbuf)
{
    track_t *t;
    audio_index_entry *e;
    uint32_t left;
    if(!AVI || AVI->mode != AVI_MODE_READ || AVI->aptr < 0 || AVI->aptr >= AVI->anum)
        return -1;
    t = &AVI->track[AVI->aptr];
    if(t->audio_posc < 0 || t->audio_posc >= t->audio_chunks) return 0;
    e = &t->audio_index[t->audio_posc];
    if(t->audio_posb < 0 || (uint32_t)t->audio_posb > e->len) return -1;
    left = e->len - (uint32_t)t->audio_posb;
    if(audbuf == NULL) return (long)left;
    if(left == 0U) {
        t->audio_posc++;
        t->audio_posb = 0;
        return 0;
    }
    if(!avi_seek(&AVI->fdes, e->pos + (uint32_t)t->audio_posb) ||
       !avi_read_exact(&AVI->fdes, audbuf, left)) {
        AVI_errno = AVI_ERR_READ;
        return -1;
    }
    t->audio_posc++;
    t->audio_posb = 0;
    return (long)left;
}

uint64_t AVI_max_size(void)
{
    return (uint64_t)AVI_MAX_FILE_SIZE;
}

long AVI_bytes_remain(avi_t *AVI)
{
    uint64_t used;
    if(!AVI || AVI->mode == AVI_MODE_READ) return 0;
    used = (uint64_t)AVI->pos + 8ULL + 16ULL *
           ((uint64_t)AVI->video_chunk_count + (uint64_t)AVI->track[0].audio_chunks +
            (uint64_t)AVI->track[1].audio_chunks);
    if(used >= AVI_MAX_FILE_SIZE) return 0;
    return (long)(AVI_MAX_FILE_SIZE - used);
}

long AVI_bytes_written(avi_t *AVI)
{
    uint64_t used;
    if(!AVI || AVI->mode == AVI_MODE_READ) return 0;
    used = (uint64_t)AVI->pos + 8ULL + 16ULL *
           ((uint64_t)AVI->video_chunk_count + (uint64_t)AVI->track[0].audio_chunks +
            (uint64_t)AVI->track[1].audio_chunks);
    if(used > 0x7FFFFFFFULL) return 0x7FFFFFFF;
    return (long)used;
}
