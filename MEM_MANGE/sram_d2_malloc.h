#ifndef _SRAM_D2_MALLOC_H_
#define _SRAM_D2_MALLOC_H_

#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* ============ 内存池配置 ============ */
#define ARRAY_MAX_SIZE         (20*1024)
#define ARRAY_SIZE             (20*1024)
#define ARRAY_ADDR_START       (0x3003B000)

/* 确保 4 字节对齐 */
#if (ARRAY_ADDR_START%4!=0)
#error "not 4-byte aligned!"
#endif

/* 确保不超出最大值 */
#if (ARRAY_SIZE>ARRAY_MAX_SIZE)
#error "size>max size!"
#endif

/* ============ API 声明 ============ */

void  sram_d2_init(void);
void *sram_d2_malloc(uint32_t size);
void  sram_d2_free(void *ptr);

uint32_t sram_d2_get_free_size(void);
uint32_t sram_d2_get_alloc_count(void);

#endif
