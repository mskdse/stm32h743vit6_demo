#ifndef _SPI_FLASH_H_
#define _SPI_FLASH_H_

#include "stm32h7xx_hal.h"

void spi_flash_w25q128_init(void);
uint8_t spi_flash_w25q128_readid(void);

#endif
