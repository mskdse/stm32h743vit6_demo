#ifndef _QSPI_FLASH_H_
#define _QSPI_FLASH_H_

#include "stm32h7xx_hal.h"

#define NOT_ENTER_QSPI_MODE   0

void qspi_flash_w25q128_init(void);
uint8_t qspi_flash_w25q128_read_id(void);

#endif
