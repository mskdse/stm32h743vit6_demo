#ifndef _AXI_MEM_MALLOC_H_
#define _AXI_MEM_MALLOC_H_

#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* ============ 内存池配置 ============ */
#define ARRAY_MAX_SIZE         (20*1024)
#define ARRAY_SIZE             (20*1024)
#define ARRAY_ADDR_START       (0x24000000+0x00080000-ARRAY_SIZE)

/* 确保 4 字节对齐 */
#if (ARRAY_ADDR_START%4!=0)
#error "not 4-byte aligned!"
#endif

/* 确保不超出最大值 */
#if (ARRAY_SIZE>ARRAY_MAX_SIZE)
#error "size>max size!"
#endif

/* ============ API 声明 ============ */

void  axi_mem_init(void);
void *axi_sram_malloc(uint32_t size);
void  axi_sram_free(void *ptr);

uint32_t axi_mem_get_free_size(void);
uint32_t axi_mem_get_alloc_count(void);

#endif
