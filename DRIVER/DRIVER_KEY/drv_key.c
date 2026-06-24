#include "drv_key.h"

typedef struct
{
    uint8_t       key_index;
    GPIO_TypeDef* gpiox;
    uint16_t      gpio_pin;
    GPIO_PinState gpio_defstate;
    uint32_t      gpio_Pull;
    uint32_t      gpio_speed;
    bool          pressstate;//按下的时候的状态
}drv_key_type;

/* 这个可以扩展，第一个成员是索引默认从0开始加，第二个成员是GPIO组，第三个成员是GPIO具体的pin */
const static drv_key_type drv_key[MAX_KEY_NUM]=
{
    {0,GPIOA,GPIO_PIN_0,GPIO_PIN_RESET,GPIO_PULLDOWN,GPIO_SPEED_FREQ_MEDIUM,true},
    {1,GPIOC,GPIO_PIN_3,GPIO_PIN_SET,GPIO_PULLUP,GPIO_SPEED_FREQ_MEDIUM,false},
    {2,GPIOC,GPIO_PIN_2,GPIO_PIN_SET,GPIO_PULLUP,GPIO_SPEED_FREQ_MEDIUM,false},
    {3,GPIOC,GPIO_PIN_0,GPIO_PIN_SET,GPIO_PULLUP,GPIO_SPEED_FREQ_MEDIUM,false}
};

void drv_key_init(void)
{
    GPIO_InitTypeDef gpio_cfg;
    gpio_cfg.Mode=GPIO_MODE_INPUT;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    for(int i=0;i<MAX_KEY_NUM;i++)
    {
        gpio_cfg.Speed=drv_key[i].gpio_speed;
        gpio_cfg.Pull=drv_key[i].gpio_Pull;
        gpio_cfg.Pin=drv_key[i].gpio_pin;
        HAL_GPIO_Init(drv_key[i].gpiox,&gpio_cfg);
        HAL_GPIO_WritePin(drv_key[i].gpiox,drv_key[i].gpio_pin,drv_key[i].gpio_defstate);
    }
}

uint32_t drv_key_readmask(void)
{
    uint32_t mask=0;
    for(int i=0;i<MAX_KEY_NUM;i++)
    {
       if(drv_key[i].pressstate==HAL_GPIO_ReadPin(drv_key[i].gpiox,drv_key[i].gpio_pin)) mask|=(1<<i);
       else                                                                              mask&=~(1<<i);
    }
    return mask;
}
