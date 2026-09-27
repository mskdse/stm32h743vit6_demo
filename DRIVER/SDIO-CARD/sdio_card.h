#ifndef _SDIO_CARD_H_
#define _SDIO_CARD_H_

#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <stdbool.h>

/* SD 卡热插拔事件类型 */
typedef enum 
{
    SDIO_SD_EVENT_NONE = 0,
    SDIO_SD_EVENT_INSERT,     /* 卡插入 */
    SDIO_SD_EVENT_REMOVE,     /* 卡拔出 */
} sdio_sd_event_t;

/* 事件回调函数类型 */
typedef void (*sdio_sd_event_cb_t)(sdio_sd_event_t event);

void sdio_sd_card_reset(void);
void sdio_sd_card_init(void);
bool sdio_sd_card_write(uint32_t start_block_num,const uint8_t* buf,uint32_t block_num);
bool sdio_sd_card_read(uint32_t start_block_num,uint8_t* buf,uint32_t block_num);
HAL_SD_CardInfoTypeDef sdio_sd_card_info(void);

void sdio_sd_card_register_cb(sdio_sd_event_cb_t cb);
void sdio_sd_card_prc_100ms(void);

#endif
