#ifndef _PWM_TIMER1_H_
#define _PWM_TIMER1_H_

#include "stm32h7xx_hal.h"

#define USE_DEMO1
//#define USE_DEMO2
//#define USE_DEMO3

#ifdef USE_DEMO1
void pwm_timer1_init(uint16_t xms,uint8_t paluse,uint8_t count);
void pwm_timer1_set_paluse_start(uint8_t paluse);
#endif

#ifdef USE_DEMO2
void pwm_timer1_init(uint16_t xms,uint8_t paluse);
void pwm_timer1_set_paluse_start(uint8_t paluse);
#endif

#ifdef USE_DEMO3
void pwm_timer1_init(uint16_t xms,uint8_t paluse);
#endif

#endif
