#include "drv_basetim.h"

static TIM_HandleTypeDef    task_tim;
static TIM_Base_InitTypeDef task_tim_cfg;
static void(*base_time_callback)(void);

void drv_basetim_init(void(*tim_1ms_clk)(void))
{
	__TIM6_CLK_ENABLE();
	
	task_tim_cfg.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
  task_tim_cfg.ClockDivision=TIM_CLOCKDIVISION_DIV1;
  task_tim_cfg.CounterMode=TIM_COUNTERMODE_UP;
  task_tim_cfg.Period=10000-1;
  task_tim_cfg.Prescaler=20000-1;
  task_tim_cfg.RepetitionCounter=0;
	
  HAL_TIM_Base_Init(&task_tim);
	
	base_time_callback=tim_1ms_clk;
	
  TIM_Base_SetConfig(TIM6,&task_tim_cfg);
	
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn,15,0);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
	
  HAL_TIM_Base_Start_IT(&task_tim);
	
  HAL_TIM_Base_Start(&task_tim);
}

void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&task_tim);
	base_time_callback();
}
