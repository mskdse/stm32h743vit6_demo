#include "axi_mem_malloc.h"

/* ================================================================
 *  空闲链表 + 首次适应 内存管理
 *  每个内存块头部 8 字节：size(4) + magic(4)
 *  空闲块头部后 4 字节存 next 指针（与已分配块的用户数据区重叠）
 *  分裂条件：剩余 >= MIN_BLOCK
 *  释放时自动合并相邻空闲块（前向 + 后向）
 *  返回地址 4 字节对齐
 * ================================================================ */

/* ---------- 内存块头部 ---------- */
typedef struct mem_block
{
    uint32_t size;          /* 块总大小(含头部) | bit0: 0=空闲 1=已分配 */
    uint32_t magic;         /* 魔数校验: 0xA5A5A5A5=空闲, 0xCDCDCDCD=已分配 */
} mem_block_header_t;

/* ---------- 全局内存控制器 ---------- */
typedef struct
{
    mem_block_header_t *free_list;      /* 空闲链表头(按地址升序) */
    uint32_t             total_size;     /* 管理的内存总量 */
    uint32_t             free_size;      /* 当前空闲字节数 */
    uint32_t             alloc_count;    /* 当前分配次数(未释放的块数) */
} axi_mem_ctl_t;

/* ---------- 常量宏 ---------- */
#define HEADER_SZ           (sizeof(mem_block_header_t))
#define ALIGN_MASK          3U
#define MIN_BLOCK           16U
#define FLAG_ALLOC          1U
#define MAGIC_FREE          0xA5A5A5A5U
#define MAGIC_ALLOC         0xCDCDCDCDU

/* ---------- 辅助函数：读写空闲块的 next 指针 ---------- */
static inline void          free_set_next(mem_block_header_t *b, mem_block_header_t *nxt)
{
    *(mem_block_header_t **)((uint8_t *)b + HEADER_SZ) = nxt;
}

static inline mem_block_header_t *free_get_next(mem_block_header_t *b)
{
    return *(mem_block_header_t **)((uint8_t *)b + HEADER_SZ);
}

/* 获取块的原始大小(清除标志位) */
static inline uint32_t      blk_raw_size(mem_block_header_t *b)
{
    return b->size & ~FLAG_ALLOC;
}

/* ---------- 静态全局控制器实例 ---------- */
static axi_mem_ctl_t mem_ctl;

/* ---------- 分散加载定义的 AXI SRAM 内存池 ---------- */
__attribute__((section(".bss.AXI_LAST"),used))
__attribute__((aligned(4)))
static uint8_t AXI_SRAM[ARRAY_SIZE];

/* ================================================================
 *                          API 实现
 * ================================================================ */

void axi_mem_init(void)
{
    mem_block_header_t *first;

    memset(AXI_SRAM, 0, ARRAY_SIZE);

    /* 整片内存初始化为一个空闲块 */
    first = (mem_block_header_t *)AXI_SRAM;
    first->size  = ARRAY_SIZE;
    first->magic = MAGIC_FREE;
    free_set_next(first, NULL);

    mem_ctl.free_list   = first;
    mem_ctl.total_size  = ARRAY_SIZE;
    mem_ctl.free_size   = ARRAY_SIZE;
    mem_ctl.alloc_count = 0;
}

/* ----------------------------------------------------------------
 *  分配：first-fit 首次适应
 *   size : 请求字节数（内部对齐到 4 的倍数）
 *   返回 : 指向用户数据区的指针（头部之后），失败返回 NULL
 * ---------------------------------------------------------------- */
void *axi_sram_malloc(uint32_t size)
{
    uint32_t need;
    uint32_t blk_size;
    uint32_t remaining;
    uint32_t alloc_size;
    mem_block_header_t *prev;
    mem_block_header_t *blk;
    mem_block_header_t *new_free;

    if(0==size) return NULL;

    /* 请求大小 4 字节对齐 */
    size = (size + ALIGN_MASK) & ~ALIGN_MASK;

    /* 最小请求 = 头部 + 用户数据，但不低于 MIN_BLOCK */
    need = HEADER_SZ + size;
    if(need < MIN_BLOCK) need = MIN_BLOCK;

    prev = NULL;
    blk  = mem_ctl.free_list;

    while(blk != NULL)
    {
        blk_size = blk_raw_size(blk);

        if(blk_size >= need)
        {
            remaining = blk_size - need;

            if(remaining >= MIN_BLOCK)
            {
                /* ---- 分裂：前半分配，后半保留为空闲 ---- */
                blk->size  = need | FLAG_ALLOC;
                blk->magic = MAGIC_ALLOC;
                alloc_size = need;

                new_free = (mem_block_header_t *)((uint8_t *)blk + need);
                new_free->size  = remaining;
                new_free->magic = MAGIC_FREE;
                free_set_next(new_free, free_get_next(blk));

                if(NULL != prev) free_set_next(prev, new_free);  
                else             mem_ctl.free_list = new_free;
            }
            else
            {
                /* ---- 整块分配 ---- */
                blk->size  = blk_size | FLAG_ALLOC;
                blk->magic = MAGIC_ALLOC;
                alloc_size = blk_size;

                if(NULL != prev) free_set_next(prev, free_get_next(blk));
                else             mem_ctl.free_list = free_get_next(blk);    
            }

            mem_ctl.free_size   -= alloc_size;
            mem_ctl.alloc_count += 1;

            return (void *)((uint8_t *)blk + HEADER_SZ);
        }

        prev = blk;
        blk  = free_get_next(blk);
    }

    return NULL;    /* 无足够空间 */
}

/* ----------------------------------------------------------------
 *  释放：回收到空闲链表 + 前后向合并
 *   ptr : axi_sram_malloc 返回的指针
 * ---------------------------------------------------------------- */
void axi_sram_free(void *ptr)
{
    mem_block_header_t *blk;
    mem_block_header_t *prev;
    mem_block_header_t *curr;
    mem_block_header_t *prev_end;
    mem_block_header_t *blk_end;
    uint32_t blk_size;
    int merged;

    if(NULL==ptr) return;

    blk = (mem_block_header_t *)((uint8_t *)ptr - HEADER_SZ);

    /* 魔数校验 */
    if(MAGIC_ALLOC!=blk->magic) return;

    blk_size = blk_raw_size(blk);

    /* 标记为空闲 */
    blk->size  = blk_size;
    blk->magic = MAGIC_FREE;

    /* ---- 按地址插入空闲链表（同时尝试合并）---- */
    prev = NULL;
    curr = mem_ctl.free_list;

    while(curr != NULL && curr < blk)
    {
        prev = curr;
        curr = free_get_next(curr);
    }

    /* 尝试与前一个空闲块合并 */
    merged = 0;
    if(NULL != prev)
    {
        prev_end = (mem_block_header_t *)((uint8_t *)prev + blk_raw_size(prev));
        if(prev_end == blk)
        {
            /* prev + blk 合并为 prev */
            prev->size = blk_raw_size(prev) + blk_size;
            free_set_next(prev, curr);
            blk    = prev;          /* 合并后的块以 prev 为准 */
            merged = 1;
        }
    }

    if(0==merged)
    {
        free_set_next(blk, curr);
        if(NULL != prev) free_set_next(prev, blk);
        else             mem_ctl.free_list = blk;  
    }

    /* 尝试与后一个空闲块合并 */
    if(curr != NULL)
    {
        blk_end = (mem_block_header_t *)((uint8_t *)blk + blk_raw_size(blk));
        if(blk_end == curr)
        {
            blk->size = blk_raw_size(blk) + blk_raw_size(curr);
            free_set_next(blk, free_get_next(curr));
        }
    }

    mem_ctl.free_size   += blk_size;
    mem_ctl.alloc_count -= 1;
}

/* ----------------------------------------------------------------
 *  查询接口
 * ---------------------------------------------------------------- */
uint32_t axi_mem_get_free_size(void)
{
    return mem_ctl.free_size;
}

uint32_t axi_mem_get_alloc_count(void)
{
    return mem_ctl.alloc_count;
}
