#include "drv_led.h"

typedef struct
{
    uint8_t       led_index;
    GPIO_TypeDef* gpiox;
    uint16_t      gpio_pin;
    GPIO_PinState gpio_defstate;
    uint32_t      gpio_Pull;
    uint32_t      gpio_speed;
}drv_led_type;

/* 这个可以扩展，第一个成员是索引默认从0开始加，第二个成员是GPIO组，第三个成员是GPIO具体的pin */
const static drv_led_type drv_led[MAX_LED_NUM]=
{
    {0,GPIOB,GPIO_PIN_0,GPIO_PIN_SET,GPIO_NOPULL,GPIO_SPEED_FREQ_MEDIUM}
};

void drv_led_init(void)
{
  GPIO_InitTypeDef gpio_cfg;
  gpio_cfg.Mode=GPIO_MODE_OUTPUT_PP;
  __HAL_RCC_GPIOB_CLK_ENABLE();
  for(int i=0;i<MAX_LED_NUM;i++)
  {
     gpio_cfg.Speed=drv_led[i].gpio_speed;
     gpio_cfg.Pull=drv_led[i].gpio_Pull;
     gpio_cfg.Pin=drv_led[i].gpio_pin;
     HAL_GPIO_Init(drv_led[i].gpiox,&gpio_cfg);
     HAL_GPIO_WritePin(drv_led[i].gpiox,drv_led[i].gpio_pin,drv_led[i].gpio_defstate);
  }
}

void drv_led_on(uint8_t led)
{
  HAL_GPIO_WritePin(drv_led[led].gpiox,drv_led[led].gpio_pin,GPIO_PIN_RESET);
}

void drv_led_off(uint8_t led)
{
HAL_GPIO_WritePin(drv_led[led].gpiox,drv_led[led].gpio_pin,GPIO_PIN_SET);
}

