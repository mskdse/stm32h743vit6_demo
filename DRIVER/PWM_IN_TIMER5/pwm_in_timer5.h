#ifndef _PWM_IN_TIMER5_H_
#define _PWM_IN_TIMER5_H_

#include "stm32h7xx_hal.h"

void pwm_in_timer5_init(void);
void pwm_in_timer5_get(uint32_t* persiod_us,uint8_t* duty);

#endif
