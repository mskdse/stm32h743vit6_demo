#include "pwm_timer1.h"

TIM_HandleTypeDef          tim1_handle;
static TIM_OC_InitTypeDef  oc_cfg;
static uint16_t            timperiod;

#ifdef USE_DEMO1
/* timer1 ch1-----PE9--->pwm 生成有限个数的pwm，生成结束之后关闭，原理是高级定时器的重复计数器功能 */ 
void pwm_timer1_init(uint16_t xms,uint8_t paluse,uint8_t count)
{
	timperiod=xms;
	
	__HAL_RCC_TIM1_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Alternate=GPIO_AF1_TIM1;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pin=GPIO_PIN_9;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOE,&gpio_cfg);
	
	tim1_handle.Instance=TIM1;
	tim1_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
	tim1_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim1_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim1_handle.Init.Period=10*xms-1;
	tim1_handle.Init.Prescaler=20000-1;
	tim1_handle.Init.RepetitionCounter=count;
	HAL_TIM_Base_Init(&tim1_handle);
	
	HAL_TIM_OC_Init(&tim1_handle);
	
	oc_cfg.OCFastMode=TIM_OCFAST_DISABLE;
	oc_cfg.OCIdleState=TIM_OCIDLESTATE_RESET;
	oc_cfg.OCMode=TIM_OCMODE_PWM1;
	oc_cfg.OCNIdleState=TIM_OCNIDLESTATE_RESET;
	oc_cfg.OCNPolarity=TIM_OCNPOLARITY_LOW;
	oc_cfg.OCPolarity=TIM_OCPOLARITY_LOW;
	oc_cfg.Pulse=(xms*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim1_handle,&oc_cfg,TIM_CHANNEL_1);
	
	HAL_TIM_OC_Start(&tim1_handle,TIM_CHANNEL_1);
	
	HAL_NVIC_SetPriority(TIM1_UP_IRQn,14,0);
	HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
	
	HAL_TIM_Base_Start_IT(&tim1_handle);
	
	HAL_TIM_Base_Start(&tim1_handle);
}
void pwm_timer1_set_paluse_start(uint8_t paluse)
{
	oc_cfg.Pulse=(timperiod*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim1_handle,&oc_cfg,TIM_CHANNEL_1);
	HAL_TIM_OC_Start(&tim1_handle,TIM_CHANNEL_1);
	HAL_TIM_Base_Start(&tim1_handle);
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(TIM1==htim->Instance)
	{
		HAL_TIM_Base_Stop(&tim1_handle);
	  HAL_TIM_OC_Stop(&tim1_handle,TIM_CHANNEL_1);
	}
}
void TIM1_UP_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&tim1_handle);
}
#endif

#ifdef USE_DEMO2
/* timer1 ch1-----PE9--->pwm 生成相位可调的PWM，占空比固定为50%，因为是PWM的翻转模式。CCR不代表占空比了，现在代表相位 */ 
void pwm_timer1_init(uint16_t xms,uint8_t paluse)
{
	timperiod=xms;
	
	__HAL_RCC_TIM1_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Alternate=GPIO_AF1_TIM1;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pin=GPIO_PIN_9;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOE,&gpio_cfg);
	
	tim1_handle.Instance=TIM1;
	tim1_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
	tim1_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim1_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim1_handle.Init.Period=10*xms-1;
	tim1_handle.Init.Prescaler=20000-1;
	tim1_handle.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim1_handle);
	
	HAL_TIM_OC_Init(&tim1_handle);
	
	oc_cfg.OCFastMode=TIM_OCFAST_DISABLE;
	oc_cfg.OCIdleState=TIM_OCIDLESTATE_RESET;
	oc_cfg.OCMode=TIM_OCMODE_TOGGLE;
	oc_cfg.OCNIdleState=TIM_OCNIDLESTATE_RESET;
	oc_cfg.OCNPolarity=TIM_OCNPOLARITY_LOW;
	oc_cfg.OCPolarity=TIM_OCPOLARITY_LOW;
	oc_cfg.Pulse=(xms*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim1_handle,&oc_cfg,TIM_CHANNEL_1);
	
	HAL_TIM_OC_Start(&tim1_handle,TIM_CHANNEL_1);
	
	HAL_TIM_Base_Start(&tim1_handle);
}
void pwm_timer1_set_paluse_start(uint8_t paluse)
{
	oc_cfg.Pulse=(timperiod*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim1_handle,&oc_cfg,TIM_CHANNEL_1);
}
#endif

#ifdef USE_DEMO3
/* timer1 ch1-----PE9--->pwm，timer1 ch1n-----PE8--->pwm，timer1 bkin-----PE15--->pwm
   生成互补输出带死区控制和刹车控制的PWM波形，PE15接到PA0按键1，按下按键之后停止输出互补波形，下个周期之后重新输出波形
   但是实际做不到，因为我按下一个按键它高电平保持时间就有169ms,加上抖动时间，这段时间会反复刹车，所以现象是按下按键然后200ms后重新输出
*/ 
void pwm_timer1_init(uint16_t xms,uint8_t paluse)
{
	__HAL_RCC_TIM1_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Alternate=GPIO_AF1_TIM1;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pin=GPIO_PIN_9|GPIO_PIN_8;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOE,&gpio_cfg);
	gpio_cfg.Pull=GPIO_PULLDOWN;
	gpio_cfg.Pin=GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE,&gpio_cfg);
	
	tim1_handle.Instance=TIM1;
	tim1_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
	tim1_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim1_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim1_handle.Init.Period=10*xms-1;
	tim1_handle.Init.Prescaler=20000-1;
	tim1_handle.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim1_handle);
	
	HAL_TIM_OC_Init(&tim1_handle);
	
	oc_cfg.OCFastMode=TIM_OCFAST_DISABLE;
	oc_cfg.OCIdleState=TIM_OCIDLESTATE_RESET;
	oc_cfg.OCMode=TIM_OCMODE_PWM1;
	oc_cfg.OCNIdleState=TIM_OCNIDLESTATE_RESET;
	oc_cfg.OCNPolarity=TIM_OCNPOLARITY_HIGH;
	oc_cfg.OCPolarity=TIM_OCPOLARITY_HIGH;
	oc_cfg.Pulse=(xms*paluse)/10;
	HAL_TIM_OC_ConfigChannel(&tim1_handle,&oc_cfg,TIM_CHANNEL_1);
	
	TIM_BreakDeadTimeConfigTypeDef  deadtime;
	deadtime.AutomaticOutput=TIM_AUTOMATICOUTPUT_ENABLE;
	deadtime.Break2Filter=0;
	deadtime.Break2Polarity=TIM_BREAK2POLARITY_LOW;
	deadtime.Break2State=TIM_BREAK2_DISABLE;
	deadtime.BreakFilter=8;
	deadtime.BreakPolarity=TIM_BREAKPOLARITY_HIGH;
	deadtime.BreakState=TIM_BREAK_ENABLE;
	deadtime.DeadTime=0xff;
	deadtime.LockLevel=TIM_LOCKLEVEL_OFF;
	deadtime.OffStateIDLEMode=TIM_OSSI_ENABLE;
	deadtime.OffStateRunMode=TIM_OSSR_ENABLE;
	HAL_TIMEx_ConfigBreakDeadTime(&tim1_handle,&deadtime);
	
	HAL_TIM_OC_Start(&tim1_handle,TIM_CHANNEL_1);
	HAL_TIMEx_OCN_Start(&tim1_handle,TIM_CHANNEL_1);
	
	HAL_TIM_Base_Start(&tim1_handle);
}

#endif
