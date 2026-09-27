#ifndef _SDIO_CARD_H_
#define _SDIO_CARD_H_

#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <stdbool.h>

void sdio_sd_card_init(void);
bool sdio_sd_card_write(uint32_t start_block_num,const uint8_t* buf,uint32_t block_num);
bool sdio_sd_card_read(uint32_t start_block_num,uint8_t* buf,uint32_t block_num);
HAL_SD_CardInfoTypeDef sdio_sd_card_info(void);

#endif
