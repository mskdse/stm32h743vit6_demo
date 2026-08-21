#include "pwm_timer2.h"
#include <string.h>

__attribute__((section (".RAM_D2")))TIM_HandleTypeDef   tim2_handle;
__attribute__((section (".RAM_D2")))static TIM_OC_InitTypeDef  oc_cfg;
__attribute__((section (".RAM_D2")))static uint16_t            timperiod;

/* timer2 ch2-----PA1--->pwm */

void pwm_timer2_init(uint16_t xms,uint8_t paluse)
{
	timperiod=xms;
	
	__HAL_RCC_TIM2_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Alternate=GPIO_AF1_TIM2;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pin=GPIO_PIN_1;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA,&gpio_cfg);
	
	memset(&tim2_handle,0,sizeof(TIM_HandleTypeDef));
	tim2_handle.Instance=TIM2;
	tim2_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
	tim2_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim2_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim2_handle.Init.Period=10*xms-1;
	tim2_handle.Init.Prescaler=20000-1;
	tim2_handle.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim2_handle);
	
	HAL_TIM_OC_Init(&tim2_handle);
	
	oc_cfg.OCFastMode=TIM_OCFAST_DISABLE;
	oc_cfg.OCIdleState=TIM_OCIDLESTATE_RESET;
	oc_cfg.OCMode=TIM_OCMODE_PWM1;
	oc_cfg.OCNIdleState=TIM_OCNIDLESTATE_RESET;
	oc_cfg.OCNPolarity=TIM_OCNPOLARITY_LOW;
	oc_cfg.OCPolarity=TIM_OCPOLARITY_LOW;
	oc_cfg.Pulse=(xms*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim2_handle,&oc_cfg,TIM_CHANNEL_2);
	
	HAL_TIM_OC_Start(&tim2_handle,TIM_CHANNEL_2);
	
	HAL_TIM_Base_Start(&tim2_handle);
}

void pwm_timer2_set_paluse(uint8_t paluse)
{
	oc_cfg.Pulse=(timperiod*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim2_handle,&oc_cfg,TIM_CHANNEL_2);
}
