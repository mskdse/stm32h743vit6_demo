#ifndef _DRV_LED_H_
#define _DRV_LED_H_

#include "stm32h7xx_hal.h"

#define MAX_LED_NUM    2

void drv_led_init(void);
void drv_led_on(uint8_t led);
void drv_led_off(uint8_t led);

#endif
