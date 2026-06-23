#ifndef _DRVP_LED_H_
#define _DRVP_LED_H_

#include "drv_led.h"
#include <stdbool.h>

typedef enum
{
  E_LED_0,
  E_LED_COUNT
}E_LED_TYPE;

void drvp_led_init(void);
void drvp_led_on(E_LED_TYPE led);
void drvp_led_off(E_LED_TYPE led);
void drvp_led_tooge_start(E_LED_TYPE led,uint32_t tms);
void drvp_led_tooge_start_for(E_LED_TYPE led,uint32_t tms,uint32_t duration);
void drvp_led_tooge_stop(E_LED_TYPE led);
void drvp_led_prc_1ms(void);

#endif


