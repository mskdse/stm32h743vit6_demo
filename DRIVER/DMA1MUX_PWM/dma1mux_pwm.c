#include "dma1mux_pwm.h"
#include <string.h>

/* 初始化能驱动DMAMUX1的请求生成器通道0的TIM12的时钟，让它产生TIM12_TRGO信号，注意这里只需要给出信号即可不需要输出到对应的复用引脚上 */
static void dma1mux_tim12_trgo_init(uint16_t prsc,uint16_t arr)
{
	__HAL_RCC_TIM12_CLK_ENABLE();
	
	TIM_HandleTypeDef tim12_cfg;
	tim12_cfg.Instance=TIM12;
	tim12_cfg.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_ENABLE;
	tim12_cfg.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim12_cfg.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim12_cfg.Init.Period=arr-1;
	tim12_cfg.Init.Prescaler=prsc-1;
	tim12_cfg.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim12_cfg);
	
	HAL_TIM_GenerateEvent(&tim12_cfg,TIM_EVENTSOURCE_TRIGGER);
	
	HAL_TIM_Base_Start(&tim12_cfg);
}

__attribute__((section (".RAM_D2")))uint32_t           dma1_ctrl_buf[8];
__attribute__((section (".RAM_D2")))DMA_HandleTypeDef  dma1_cfg;

void dma1mux_pwm_init(void)
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
	
	/* 配置它的BDMA功能 */
	memset(&bdma_cfg,0,sizeof(DMA_HandleTypeDef));
	bdma_cfg.Instance=BDMA_Channel0;
	bdma_cfg.Init.Direction=DMA_MEMORY_TO_PERIPH;//方向来自于内存到外设，其实本例子是DMA控制GPIO用m2m也是可以的
	bdma_cfg.Init.FIFOMode=DMA_FIFOMODE_DISABLE;//该BDMA没有FIFO机制
	bdma_cfg.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;//随便给，反正没有
	bdma_cfg.Init.MemBurst=DMA_MBURST_SINGLE;//该DMA没有突发功能
	bdma_cfg.Init.MemDataAlignment=DMA_MDATAALIGN_WORD;//GPIO的BSRR寄存器是字所以给字
	bdma_cfg.Init.MemInc=DMA_MINC_ENABLE;//内存地址递增
	bdma_cfg.Init.Mode=DMA_CIRCULAR;//循环模式
	bdma_cfg.Init.PeriphBurst=DMA_PBURST_SINGLE;//该DMA没有突发功能
	bdma_cfg.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;//GPIO的BSRR寄存器是字所以给字
	bdma_cfg.Init.PeriphInc=DMA_PINC_DISABLE;//外设地址不递增
	bdma_cfg.Init.Priority=DMA_PRIORITY_MEDIUM;//优先级随便给
	bdma_cfg.Init.Request=DMA_REQUEST_GENERATOR0;//DMA请求来自于请求发生器通道0，而非外设
	HAL_DMA_Init(&bdma_cfg);
	
	/* 配置DMAMUX，使其请求生成器通道0接收LPTIM2_OUT信号 */
	HAL_DMA_MuxRequestGeneratorConfigTypeDef dma_mux_req_cfg;
	memset(&dma_mux_req_cfg,0,sizeof(HAL_DMA_MuxRequestGeneratorConfigTypeDef));
	dma_mux_req_cfg.Polarity=HAL_DMAMUX_REQ_GEN_RISING_FALLING;//LPTIM2_OUT信号双边沿都能触发DMA请求，也就是说50us搬运一次
	dma_mux_req_cfg.RequestNumber=1;//触发后DMA传输一次
	dma_mux_req_cfg.SignalID=HAL_DMAMUX2_REQ_GEN_LPTIM2_OUT;//接收LPTIM2_OUT信号
	HAL_DMAEx_ConfigMuxRequestGenerator(&bdma_cfg,&dma_mux_req_cfg);//配置触发输入请求而非同步输入请求
	HAL_DMAEx_EnableMuxRequestGenerator(&bdma_cfg);
	
	/* 配置要传输到GPIOD_BSRR的数据,这里的话要对称一下，因为是双缓冲，前面4个和后面4个一样 */	
	bdma_ctrl_buf[0]=0x00000002;//PB1=1;
	bdma_ctrl_buf[1]=0x00020000;//PB1=0;
	bdma_ctrl_buf[2]=0x00000002;//PB1=1;
	bdma_ctrl_buf[3]=0x00020000;//PB1=0;
	
	bdma_ctrl_buf[4]=0x00000002;
	bdma_ctrl_buf[5]=0x00020000;
	bdma_ctrl_buf[6]=0x00000002;
	bdma_ctrl_buf[7]=0x00020000;
	HAL_DMA_Start_IT(&bdma_cfg,((uint32_t)bdma_ctrl_buf),((uint32_t)(&GPIOB->BSRR)),8);
	
	/* 打开全满半满中断和错误中断,HAL库内部会清空半满中断使能，所以我们放这 */
	BDMA_Channel0->CCR|=(0x01<<1);
	BDMA_Channel0->CCR|=(0x01<<2);
	BDMA_Channel0->CCR|=(0x01<<3);
	BDMA->IFCR|=(0x01<<1);
	BDMA->IFCR|=(0x01<<2);
	BDMA->IFCR|=(0x01<<3);
	HAL_NVIC_SetPriority(BDMA_Channel0_IRQn,12,0);
	HAL_NVIC_EnableIRQ(BDMA_Channel0_IRQn);
	
	dma1mux_tim12_trgo_init(,);//提供触发信号
}

void DMA1_Stream0_IRQHandler(void)
{
  if(BDMA->ISR&(0x01<<1))//全满中断
	{
		bdma_ctrl_buf[4]=0x00020000;
		bdma_ctrl_buf[5]=0x00000002;
		bdma_ctrl_buf[6]=0x00000002;
		bdma_ctrl_buf[7]=0x00020000;
		BDMA->IFCR|=(0x01<<1);
	}
	
	if(BDMA->ISR&(0x01<<2))//半满中断
	{
		bdma_ctrl_buf[0]=0x00000002;
		bdma_ctrl_buf[1]=0x00000002;
		bdma_ctrl_buf[2]=0x00000002;
		bdma_ctrl_buf[3]=0x00020000;
		BDMA->IFCR|=(0x01<<2);
	}
	
	if(BDMA->ISR&(0x01<<3))//错误中断
	{
		BDMA->IFCR|=(0x01<<3);
	}
}
