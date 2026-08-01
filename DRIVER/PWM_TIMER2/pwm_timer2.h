#ifndef _PWM_TIMER2_H_
#define _PWM_TIMER2_H_

#include "stm32h7xx_hal.h"


void pwm_timer2_init(uint16_t xms,uint8_t paluse);
void pwm_timer2_set_paluse(uint8_t paluse);

#endif
