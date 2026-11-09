#ifndef _DRVP_KEY_H_
#define _DRVP_KEY_H_

#include "stm32h7xx_hal.h"

#define WM8978_DEVADDR_7BIT    0x1A
#define WM8978_DEVADDR_8BIT   ((WM8978_DEVADDR_7BIT<<1)|0)

void wm8978_borad_init(void);
void wm8978_init(void);

#endif
