#include "dma1mux_pwm.h"
#include <string.h>

__attribute__((section (".RAM_D2")))TIM_HandleTypeDef tim12_cfg;

/* 初始化能驱动DMAMUX1的请求生成器通道0的TIM12的时钟，让它产生TIM12_TRGO信号，注意这里只需要给出信号即可不需要输出到对应的复用引脚上 */
static void dma1mux_tim12_trgo_init(uint16_t prsc,uint16_t arr)
{
	__HAL_RCC_TIM12_CLK_ENABLE();
	
	memset(&tim12_cfg,0,sizeof(TIM_HandleTypeDef));
	tim12_cfg.Instance=TIM12;
	tim12_cfg.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_ENABLE;
	tim12_cfg.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim12_cfg.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim12_cfg.Init.Period=arr-1;
	tim12_cfg.Init.Prescaler=prsc-1;
	tim12_cfg.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim12_cfg);
	
	TIM_MasterConfigTypeDef master_cfg;
  memset(&master_cfg, 0, sizeof(master_cfg));
  master_cfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
  master_cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&tim12_cfg, &master_cfg);
	
	HAL_TIM_Base_Start(&tim12_cfg);
}

/* 一个高电平一个低电平能充当一个脉冲所以这里最大输出DMA_PWM_MAX_CNT个脉冲 */
#define DMA_PWM_MAX_CNT  8
#define DMA_PWM_SET      0x00000002  //PX1=1
#define DMA_PWM_RESET    0x00020000  //PX1=0
__attribute__((section (".RAM_D2")))uint32_t           dma1_ctrl_buf[DMA_PWM_MAX_CNT*2];
__attribute__((section (".RAM_D2")))DMA_HandleTypeDef  dma1_cfg;
__attribute__((section (".RAM_D2")))volatile uint8_t   target_cnt;
__attribute__((section (".RAM_D2")))volatile uint8_t   current_cnt;

static void dma1mux_fill_buf(uint32_t *buf, uint8_t cnt) 
{ 
	for(int i = 0; i < (DMA_PWM_MAX_CNT/2); i++) 
	{ 
		if(i < cnt) 
		{ 
			buf[i * 2] =     DMA_PWM_SET; 
			buf[i * 2 + 1] = DMA_PWM_RESET; 
		} 
		else 
		{ 
			buf[i * 2] =     DMA_PWM_RESET; 
			buf[i * 2 + 1] = DMA_PWM_RESET; 
		} 
	} 
}

void dma1mux_pwm_init(uint8_t cnt)
{
		__HAL_RCC_DMA1_CLK_ENABLE();
		__HAL_RCC_GPIOB_CLK_ENABLE();
		
		/* 配置我要控制的引脚PD3，不需要复用 */
		GPIO_InitTypeDef gpio_init_structure;
		gpio_init_structure.Mode      = GPIO_MODE_OUTPUT_PP;
		gpio_init_structure.Pull      = GPIO_NOPULL;
		gpio_init_structure.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init_structure.Pin       = GPIO_PIN_1;
		HAL_GPIO_Init(GPIOB, &gpio_init_structure);
		
		/* 配置它的DMA功能 */
		memset(&dma1_cfg,0,sizeof(DMA_HandleTypeDef));
		dma1_cfg.Instance=DMA1_Stream0;
		dma1_cfg.Init.Direction=DMA_MEMORY_TO_PERIPH;//方向来自于内存到外设，其实本例子是DMA控制GPIO用m2m也是可以的
		dma1_cfg.Init.FIFOMode=DMA_FIFOMODE_DISABLE;//不需要突发，FIFO无意义
		dma1_cfg.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;//随便给，反正没有
		dma1_cfg.Init.MemBurst=DMA_MBURST_SINGLE;//无需突发功能
		dma1_cfg.Init.MemDataAlignment=DMA_MDATAALIGN_WORD;//GPIO的BSRR寄存器是字所以给字
		dma1_cfg.Init.MemInc=DMA_MINC_ENABLE;//内存地址递增
		dma1_cfg.Init.Mode=DMA_CIRCULAR;//循环模式
		dma1_cfg.Init.PeriphBurst=DMA_PBURST_SINGLE;//无需突发功能
		dma1_cfg.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;//GPIO的BSRR寄存器是字所以给字
		dma1_cfg.Init.PeriphInc=DMA_PINC_DISABLE;//外设地址不递增
		dma1_cfg.Init.Priority=DMA_PRIORITY_MEDIUM;//优先级随便给
		dma1_cfg.Init.Request=DMA_REQUEST_GENERATOR0;//DMA请求来自于请求发生器通道0，而非外设
		HAL_DMA_Init(&dma1_cfg);
		
		/* 配置DMAMUX，使其请求生成器通道0接收LPTIM2_OUT信号 */
		HAL_DMA_MuxRequestGeneratorConfigTypeDef dma_mux_req_cfg;
		memset(&dma_mux_req_cfg,0,sizeof(HAL_DMA_MuxRequestGeneratorConfigTypeDef));
		dma_mux_req_cfg.Polarity=HAL_DMAMUX_REQ_GEN_RISING;//触发信号的上升沿触发一次
		dma_mux_req_cfg.RequestNumber=1;//触发后DMA传输一次
		dma_mux_req_cfg.SignalID=HAL_DMAMUX1_REQ_GEN_TIM12_TRGO;//接收TIM12_TRGO信号
		HAL_DMAEx_ConfigMuxRequestGenerator(&dma1_cfg,&dma_mux_req_cfg);//配置触发输入请求而非同步输入请求
		HAL_DMAEx_EnableMuxRequestGenerator(&dma1_cfg);
		
		/* 配置要传输到GPIOD_BSRR的数据,这里的话要对称一下，因为是双缓冲，前面8个和后面8个一样 */	
		current_cnt=   0;
		target_cnt = cnt;
		if((cnt*2)<=DMA_PWM_MAX_CNT)
		{
			dma1mux_fill_buf(&dma1_ctrl_buf[0],cnt);
			dma1mux_fill_buf(&dma1_ctrl_buf[DMA_PWM_MAX_CNT],0);
		}
		else
		{
			dma1mux_fill_buf(&dma1_ctrl_buf[0],(DMA_PWM_MAX_CNT/2));
			dma1mux_fill_buf(&dma1_ctrl_buf[DMA_PWM_MAX_CNT],(cnt-(DMA_PWM_MAX_CNT/2)));
		}
		
		HAL_DMA_Start_IT(&dma1_cfg,((uint32_t)dma1_ctrl_buf),((uint32_t)(&GPIOB->BSRR)),(DMA_PWM_MAX_CNT*2));
		
		/* 打开全满半满中断和错误中断,HAL库内部会清空半满中断使能，所以我们放这 */
		DMA1_Stream0->CR|=(0x01<<2);
		DMA1_Stream0->CR|=(0x01<<3);
		DMA1_Stream0->CR|=(0x01<<4);
		DMA1->LIFCR|=(0x01<<3);
		DMA1->LIFCR|=(0x01<<4);
		DMA1->LIFCR|=(0x01<<5);
		HAL_NVIC_SetPriority(DMA1_Stream0_IRQn,12,0);
		HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
		
		dma1mux_tim12_trgo_init(2000,100);//提供触发信号
}

void dma1mux_pwm_set_cnt(uint8_t cnt)
{
	__disable_irq();
	HAL_DMA_Abort(&dma1_cfg);
	if((cnt*2)<=DMA_PWM_MAX_CNT)
	{
		dma1mux_fill_buf(&dma1_ctrl_buf[0],cnt);
		dma1mux_fill_buf(&dma1_ctrl_buf[DMA_PWM_MAX_CNT],0);
	}
	else
	{
		dma1mux_fill_buf(&dma1_ctrl_buf[0],(DMA_PWM_MAX_CNT/2));
		dma1mux_fill_buf(&dma1_ctrl_buf[DMA_PWM_MAX_CNT],(cnt-(DMA_PWM_MAX_CNT/2)));
	}
	current_cnt= 0;
	target_cnt=cnt;
	HAL_TIM_Base_Start(&tim12_cfg);
	HAL_DMA_Start_IT(&dma1_cfg,((uint32_t)dma1_ctrl_buf),((uint32_t)(&GPIOB->BSRR)),(DMA_PWM_MAX_CNT*2));
	__enable_irq();
}

void DMA1_Stream0_IRQHandler(void)
{
  if(DMA1->LISR&(0x01<<5))//全满中断
	{
		  DMA1->LIFCR|=(0x01<<5);
		  DMA1->LIFCR|=(0x01<<4);
		  if(target_cnt!=0) 
			{
				dma1mux_fill_buf(&dma1_ctrl_buf[DMA_PWM_MAX_CNT],0);
				target_cnt=0;
				current_cnt=target_cnt;
				HAL_TIM_Base_Stop(&tim12_cfg);
			}
	}
	
	if(DMA1->LISR&(0x01<<4))//半满中断
	{
		  DMA1->LIFCR|=(0x01<<4);
		  if(current_cnt!=target_cnt) 
			{
				dma1mux_fill_buf(&dma1_ctrl_buf[0],0);
				current_cnt=target_cnt;
			}		
	}
	
	if(DMA1->LISR&(0x01<<3))//错误中断
	{
		DMA1->LIFCR|=(0x01<<3);
	}
}
