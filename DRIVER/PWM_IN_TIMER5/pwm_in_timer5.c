#include "pwm_in_timer5.h" 

TIM_HandleTypeDef tim5_handle; 

void pwm_in_timer5_init(void) 
{ 
		__HAL_RCC_TIM5_CLK_ENABLE(); 
		__HAL_RCC_GPIOA_CLK_ENABLE(); 
	
		GPIO_InitTypeDef gpio_cfg; 
		gpio_cfg.Alternate=GPIO_AF2_TIM5; 
		gpio_cfg.Mode=GPIO_MODE_AF_PP; 
		gpio_cfg.Pin=GPIO_PIN_0; 
		gpio_cfg.Pull=GPIO_PULLUP; 
		gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH; 
		HAL_GPIO_Init(GPIOA,&gpio_cfg); 
	
	  tim5_handle.Instance=TIM5;
		tim5_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
		tim5_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
		tim5_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
		tim5_handle.Init.Period=65536-1;
		tim5_handle.Init.Prescaler=200-1;
		tim5_handle.Init.RepetitionCounter=0;
		HAL_TIM_Base_Init(&tim5_handle);
	
		HAL_TIM_IC_Init(&tim5_handle); 
	  
	  TIM_IC_InitTypeDef iConfig; 
		iConfig.ICFilter=0; 
		iConfig.ICPolarity=TIM_ICPOLARITY_RISING; 
		iConfig.ICPrescaler=TIM_ICPSC_DIV1; 
		iConfig.ICSelection=TIM_ICSELECTION_DIRECTTI;//TI1FP1---TI1
		HAL_TIM_IC_ConfigChannel(&tim5_handle,&iConfig,TIM_CHANNEL_1); 
		iConfig.ICPolarity=TIM_ICPOLARITY_FALLING;
		iConfig.ICSelection=TIM_ICSELECTION_INDIRECTTI;//TI1FP2---TI1
		HAL_TIM_IC_ConfigChannel(&tim5_handle,&iConfig,TIM_CHANNEL_2);
		
		TIM_SlaveConfigTypeDef  slave_cfg;
		slave_cfg.InputTrigger=TIM_TS_TI1FP1;
		slave_cfg.SlaveMode=TIM_SLAVEMODE_RESET;
		slave_cfg.TriggerFilter=0;
		slave_cfg.TriggerPolarity=TIM_TRIGGERPOLARITY_NONINVERTED;
		slave_cfg.TriggerPrescaler=TIM_TRIGGERPRESCALER_DIV1;
		HAL_TIM_SlaveConfigSynchro(&tim5_handle,&slave_cfg);
		
		HAL_NVIC_SetPriority(TIM5_IRQn,14,0);
	  HAL_NVIC_EnableIRQ(TIM5_IRQn);
		
		HAL_TIM_IC_Start(&tim5_handle, TIM_CHANNEL_1);      // CH1 不开中断
    HAL_TIM_IC_Start_IT(&tim5_handle, TIM_CHANNEL_2);   // CH2 开中断
} 

volatile uint32_t period;
volatile uint32_t high;

//之所以只开通道2下降沿的中断，因为产生下降沿中断的时候其实CCR1早在之前的上升沿被锁存了，然后本次下降沿中断直接一次性读两个

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM5 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        period = TIM5->CCR1;
        high   = TIM5->CCR2;
    }
}

void TIM5_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&tim5_handle);
}

void pwm_in_timer5_get(uint32_t* persiod_us,uint8_t* duty) 
{ 
	*persiod_us=(period+1);
	*duty=(high+1)*100 / (period+1);
} 
