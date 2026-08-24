#ifndef _QSPI_FLASH_H_
#define _QSPI_FLASH_H_

#include "stm32h7xx_hal.h"

#define  W25Q128_SIZE  (16*1024*1024)
#define  SECTOR_SIZE            4096
#define  SECTOR_SIZE_BIT          12  //4096是2的12次方
#define  PAGE_SIZE               256

#define  FLASH_ID             0x4018
#define  FLASH_ID_MASK        0xFFFF

void qspi_flash_w25q128_init(void);
uint32_t qspi_flash_w25q128_read_id(void);
void qspi_flash_erase_sector(uint32_t addr);
void qspi_flash_read(uint32_t addr,uint8_t* data,uint32_t len);
void qspi_flash_write(uint32_t addr,uint8_t* data,uint32_t len);

#endif
