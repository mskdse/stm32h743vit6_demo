#include "base_timer6.h"

static void (*timer6_callback)(void);
static TIM_HandleTypeDef tim6_handle;

void base_timer6_init(uint16_t xms,void(*tim_calbk)(void))
{
	__HAL_RCC_TIM6_CLK_ENABLE();
	
	tim6_handle.Instance=TIM6;
	tim6_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
	tim6_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim6_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim6_handle.Init.Period=10*xms-1;
	tim6_handle.Init.Prescaler=20000-1;
	tim6_handle.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim6_handle);
	
	timer6_callback=tim_calbk;
	
	HAL_NVIC_SetPriority(TIM6_DAC_IRQn,15,0);
	HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
	
	HAL_TIM_Base_Start_IT(&tim6_handle);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(TIM6==htim->Instance) timer6_callback();
}

void TIM6_DAC_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&tim6_handle);
}
