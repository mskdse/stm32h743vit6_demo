#ifndef _DRV_KEY_H_
#define _DRV_KEY_H_

#include "stm32h7xx.h"
#include <stdbool.h>

#define MAX_KEY_NUM     4

void drv_key_init(void);
uint32_t drv_key_readmask(void);

#endif
