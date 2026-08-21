#include "drvp_led.h"

typedef struct
{
   void(*init)(void);
   void(*on)(E_LED_TYPE);
   void(*off)(E_LED_TYPE);
}drv_led_type;

typedef struct
{
   volatile bool toogleflag;
   volatile bool stopflag;
   bool  status;
   uint32_t tooglems;
   uint32_t timer;
   volatile uint32_t runtime;
}drvp_led_type;

const static drv_led_type drv_led=
{
  .init=drv_led_init,
  .on=drv_led_on,
  .off=drv_led_off
};

__attribute__((section (".RAM_D2")))static drvp_led_type drvp_led[E_LED_COUNT];

void drvp_led_init(void)
{
    drv_led.init();
}

void drvp_led_on(E_LED_TYPE led)
{
    drvp_led[led].status=true;
    drv_led.on(led);
}

void drvp_led_off(E_LED_TYPE led)
{
    drvp_led[led].status=false;
    drv_led.off(led);
}

void drvp_led_tooge_start(E_LED_TYPE led,uint32_t tms)
{
    drvp_led[led].timer = 0U;
    drvp_led[led].toogleflag = true;
    drvp_led[led].tooglems = tms;
    drvp_led[led].runtime = 0U;
}

void drvp_led_tooge_start_for(E_LED_TYPE led,uint32_t tms,uint32_t duration)
{
    drvp_led[led].timer = 0U;
    drvp_led[led].toogleflag = true;
    drvp_led[led].tooglems = tms;
    drvp_led[led].runtime = duration;
}

void drvp_led_tooge_stop(E_LED_TYPE led)
{
    drvp_led[led].toogleflag = false;
    drvp_led[led].timer = 0U;
    drvp_led[led].stopflag = true;

    if (drvp_led[led].status)
    {
        drvp_led[led].status = false;
        drv_led.off(led);
    }
}

void drvp_led_prc_1ms(void)
{
    for(E_LED_TYPE i = E_LED_0; i < E_LED_COUNT; i++)
    {
        if (drvp_led[i].stopflag)
        {
            drvp_led[i].stopflag = false;
            if (drvp_led[i].status)
            {
                drv_led.off(i);
                drvp_led[i].status = false;
            }
        }
        else if (drvp_led[i].toogleflag)
        {
            if (drvp_led[i].runtime > 0U)
            {
                drvp_led[i].runtime--;
                if (drvp_led[i].runtime == 0U)
                {
                    drvp_led[i].toogleflag = false;
                    drvp_led[i].timer = 0U;
                    if (drvp_led[i].status)
                    {
                        drv_led.off(i);
                        drvp_led[i].status = false;
                    }
                    continue;
                }
            }
            drvp_led[i].timer++;
            if (drvp_led[i].timer >= drvp_led[i].tooglems)
            {
                drvp_led[i].timer = 0U;
                if (drvp_led[i].status)
                {
                    drv_led.off(i);
                    drvp_led[i].status = false;
                }
                else
                {
                    drv_led.on(i);
                    drvp_led[i].status = true;
                }
            }
        }
    }
}

