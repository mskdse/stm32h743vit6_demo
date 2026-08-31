#include "drv_lptimer2.h"
#include <string.h>

/* lptimer2_out---PB13 */
__attribute__((section (".RAM_D2")))static LPTIM_HandleTypeDef lptimer2_handle;

void drv_lptimer2_init(void)
{
	/* 摘抄SDK中的时钟复位以及初始化，不要自己手搓 */
	__HAL_RCC_LPTIM2_CLK_ENABLE();
	__HAL_RCC_LPTIM2_FORCE_RESET();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_LPTIM2_RELEASE_RESET();
	
	/* Configure PB.13 */
	GPIO_InitTypeDef     GPIO_InitStruct;
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  GPIO_InitStruct.Alternate = GPIO_AF3_LPTIM2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	/* 摘抄SDK中的LPTIM2时钟源选择，总线时钟来源于APB4-100MHZ */
	RCC_PeriphCLKInitTypeDef        RCC_PeriphCLKInitStruct;
	RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM2;
  RCC_PeriphCLKInitStruct.Lptim2ClockSelection = RCC_LPTIM2CLKSOURCE_PCLK4;
  HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
	
	memset(&lptimer2_handle,0,sizeof(LPTIM_HandleTypeDef));
	lptimer2_handle.Instance=LPTIM2;
	lptimer2_handle.Init.Clock.Prescaler=LPTIM_PRESCALER_DIV1;//时钟分频
	lptimer2_handle.Init.Clock.Source=LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;//时钟源来自内部
	lptimer2_handle.Init.CounterSource=LPTIM_COUNTERSOURCE_INTERNAL;//计数源来自内部
	//lptimer2_handle.Init.Input1Source=;没有用输入功能
	//lptimer2_handle.Init.Input2Source=;
	lptimer2_handle.Init.OutputPolarity=LPTIM_OUTPUTPOLARITY_LOW;//极性选择
	//lptimer2_handle.Init.Trigger.ActiveEdge=;外部触发的参数一律不管
	//lptimer2_handle.Init.Trigger.SampleTime=;
	lptimer2_handle.Init.Trigger.Source=LPTIM_TRIGSOURCE_SOFTWARE;//软件启动计数
	//lptimer2_handle.Init.UltraLowPowerClock.Polarity=;外部触发的参数一律不管
	//lptimer2_handle.Init.UltraLowPowerClock.SampleTime=;
	lptimer2_handle.Init.UpdateMode=LPTIM_UPDATE_ENDOFPERIOD;//使能定时器寄存器缓存，更新周期到了再更新ARR改变值
	HAL_LPTIM_Init(&lptimer2_handle);
	
	HAL_LPTIM_PWM_Start(&lptimer2_handle,100-1,30-1);
}
