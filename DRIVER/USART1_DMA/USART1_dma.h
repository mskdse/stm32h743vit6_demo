#ifndef _USART1_DMA_H_
#define _USART1_DMA_H_

#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

void usart1_dma_init(uint32_t badue);
void usart1_my_printf(const char *format, ...);
bool usart1_fifo_out(uint8_t* buf,uint16_t* size);

#endif
