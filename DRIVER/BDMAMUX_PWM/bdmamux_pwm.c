#include "bdmamux_pwm.h"
#include <string.h>

/* 初始化能驱动DMAMUX2的请求生成器通道0的LTIM2的时钟，让它产生lptim2_out信号，注意这里只需要给出信号即可不需要输出到对应的复用引脚上 */
static void bdmamux_lptim2_init(void)
{
	/* 摘抄SDK中的时钟复位以及初始化，不要自己手搓 */
	__HAL_RCC_LPTIM2_CLK_ENABLE();
	__HAL_RCC_LPTIM2_FORCE_RESET();
	__HAL_RCC_LPTIM2_RELEASE_RESET();
	
	/* 摘抄SDK中的LPTIM2时钟源选择，总线时钟来源于APB4-100MHZ */
	RCC_PeriphCLKInitTypeDef        RCC_PeriphCLKInitStruct;
	RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LPTIM2;
  RCC_PeriphCLKInitStruct.Lptim2ClockSelection = RCC_LPTIM2CLKSOURCE_PCLK4;
  HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);
	
	LPTIM_HandleTypeDef lptimer2_handle;
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
	
	HAL_LPTIM_PWM_Start(&lptimer2_handle,10000-1,5000-1);//生成10KHZ，占空比为50%的PWM_OUT信号
}

__attribute__((aligned(4))) __attribute__((section(".RAM_D3"))) uint32_t bdma_ctrl_buf[8];//一定要是D3域的，否则BDMA没有通路，无法传输
__attribute__((section (".RAM_D2")))DMA_HandleTypeDef  bdma_cfg;

void bdmamux_pwm_init(void)
{
  __HAL_RCC_BDMA_CLK_ENABLE();
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
	
	bdmamux_lptim2_init();//提供触发信号
}

void BDMA_Channel0_IRQHandler(void)
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
