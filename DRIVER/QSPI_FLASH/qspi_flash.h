#ifndef _QSPI_FLASH_H_
#define _QSPI_FLASH_H_

#include "stm32h7xx_hal.h"

/* W25Q128出厂不是QSPI模式，借助这个宏定义下载一次代码刷一次，之后一直保持0即可 */
#define NOT_ENTER_QSPI_MODE   0

void qspi_flash_w25q128_init(void);
uint32_t qspi_flash_w25q128_read_id(void);
void qspi_flash_erase_sector(uint32_t addr);

#endif
