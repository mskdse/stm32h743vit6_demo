#ifndef _DRV_BASETIM_H_
#define _DRV_BASETIM_H_

#include "stm32h7xx_hal.h"

void drv_basetim_init(void(*tim_1ms_clk)(void));

#endif
