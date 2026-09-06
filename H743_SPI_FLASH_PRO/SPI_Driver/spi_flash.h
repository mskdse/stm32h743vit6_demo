#ifndef _SPI_FLASH_H_
#define _SPI_FLASH_H_

#include "stm32h7xx_hal.h"

void spi_flash_w25q128_init(void);
uint32_t spi_flash_w25q128_read_id(void);
void spi_flash_erase_sector_4k(uint32_t addr);
void spi_flash_erase_sector_32k(uint32_t addr);
void spi_flash_erase_sector_64k(uint32_t addr);
void spi_flash_erase_chip(void);
void spi_flash_read(uint32_t addr,uint8_t* data,uint32_t len);
void spi_flash_write(uint32_t addr,uint8_t* data,uint32_t len);

#endif
