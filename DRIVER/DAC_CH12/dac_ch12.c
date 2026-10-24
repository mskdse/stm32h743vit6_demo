#include "dac_ch12.h"
#include <string.h>

#define DAC_CH12_DMA_BUF_SIZE      1000
__attribute__((section(".RAM_D1"), aligned(32)))uint32_t DAC_CH12_DMA_BUF[2][DAC_CH12_DMA_BUF_SIZE];
__attribute__((section (".RAM_D2")))volatile uint8_t     DAC_CH12_USE_BUF_INDEX;//当前使用的是哪个buffer

__attribute__((section (".RAM_D2")))static TIM_HandleTypeDef tim7_handle;

static void dac_ch12_borad_init(void)
{
	GPIO_InitTypeDef          GPIO_InitStruct;
	
	memset(&DAC_CH12_DMA_BUF[0],0,sizeof(DAC_CH12_DMA_BUF[0]));
	memset(&DAC_CH12_DMA_BUF[1],0,sizeof(DAC_CH12_DMA_BUF[1]));
	memset(&tim7_handle,0,sizeof(tim7_handle));
	DAC_CH12_USE_BUF_INDEX=0;
	
	for(int i=0;i<100;i++)     DAC_CH12_DMA_BUF[0][i]=0;
	for(int i=100;i<200;i++)   DAC_CH12_DMA_BUF[0][i]=(4095<<16)|4095;
	
	for(int i=200;i<300;i++)   DAC_CH12_DMA_BUF[0][i]=0;
	for(int i=300;i<400;i++)   DAC_CH12_DMA_BUF[0][i]=(4095<<16)|4095;
	
	for(int i=400;i<500;i++)   DAC_CH12_DMA_BUF[0][i]=0;
	for(int i=500;i<600;i++)   DAC_CH12_DMA_BUF[0][i]=(4095<<16)|4095;
	
	for(int i=600;i<700;i++)   DAC_CH12_DMA_BUF[0][i]=0;
	for(int i=700;i<800;i++)   DAC_CH12_DMA_BUF[0][i]=(4095<<16)|4095;
	
	for(int i=800;i<900;i++)   DAC_CH12_DMA_BUF[0][i]=0;
	for(int i=900;i<1000;i++)  DAC_CH12_DMA_BUF[0][i]=(4095<<16)|4095;

  /*##-1- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO clock ****************************************/
  DACx_CHANNEL_GPIO_CLK_ENABLE();
  /* DAC Periph clock enable */
  DACx_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();
	__HAL_RCC_TIM7_CLK_ENABLE();

  /*##-2- Configure peripheral GPIO ##########################################*/
  /* DAC Channel1 and Channel2 GPIO pin configuration */
  GPIO_InitStruct.Pin = (DACx_CHANNEL_PIN | DACx_CHANNEL_PIN2);
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DACx_CHANNEL_GPIO_PORT, &GPIO_InitStruct);
}

void dac_ch12_init(void)
{
	dac_ch12_borad_init();
	
	DAC1->CR&=~(0x01<<30);//正常运行模式
	DAC1->CR&=~(0x01<<29);//禁止下溢中断
	DAC1->CR|=(0x01<<28);//使能DMA
	DAC1->CR&=~(0x01<<23);//禁止生成噪声波和三角波
	DAC1->CR&=~(0x01<<22);
	DAC1->CR&=~(0x01<<21);//触发信号为TM7_TRG0
	DAC1->CR|=(0x01<<20);
	DAC1->CR|=(0x01<<19);
	DAC1->CR&=~(0x01<<18);
	DAC1->CR|=(0x01<<17);//使能通道2触发功能，由触发信号触发的意思
	
	
	DAC1->CR&=~(0x01<<14);//正常运行模式
	DAC1->CR&=~(0x01<<13);//禁止下溢中断
	DAC1->CR|=(0x01<<12);//使能DMA
	DAC1->CR&=~(0x01<<7);//禁止生成噪声波和三角波
	DAC1->CR&=~(0x01<<6);
	DAC1->CR&=~(0x01<<5);//触发信号也为TM7_TRG0
	DAC1->CR|=(0x01<<4);
	DAC1->CR|=(0x01<<3);
	DAC1->CR&=~(0x01<<2);
	DAC1->CR|=(0x01<<1);//使能通道1触发功能，由触发信号触发的意思
	
	DAC1->MCR=0;//通道1和2的缓冲功能全部开启，并且只连接到外部引脚
	
	DAC1->CR|=(0x01<<16);//使能通道2
	DAC1->CR|=(0x01<<0);//使能通道1
	
	DMA_HandleTypeDef hdma;
	hdma.Instance=DMA1_Stream7;
	hdma.Init.Direction=DMA_MEMORY_TO_PERIPH;//内存到外设，通过DAC输出缓冲区数据
	hdma.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
	hdma.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	hdma.Init.MemBurst=DMA_MBURST_SINGLE;
	hdma.Init.MemDataAlignment=DMA_MDATAALIGN_WORD;//双DAC使用同一个定时器产生触发信号。
	hdma.Init.MemInc=DMA_MINC_ENABLE;
	hdma.Init.Mode=DMA_NORMAL;//单次模式配合软件双缓冲
	hdma.Init.PeriphBurst=DMA_PBURST_SINGLE;
	hdma.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;//使用双右对齐寄存器是32位
	hdma.Init.PeriphInc=DMA_PINC_DISABLE;
	hdma.Init.Priority=DMA_PRIORITY_VERY_HIGH;
	hdma.Init.Request=DMA_REQUEST_DAC1_CH1;//SDK写错了，DMA_REQUEST_DAC1_CH1是DAC1的意思
	HAL_DMA_Init(&hdma);
	
	DMA1_Stream7->CR|=(0x01<<4);//开启传输完成中断
	DMA1_Stream7->NDTR=DAC_CH12_DMA_BUF_SIZE;//一个周期的波形
	DMA1_Stream7->PAR=(uint32_t)&DAC1->DHR12RD;//外设地址,双DAC通道右对齐
	DMA1_Stream7->M0AR=(uint32_t)DAC_CH12_DMA_BUF[0];//内存地址,给缓冲区的地址
	DMA1_Stream7->CR|=(0x01<<0);//开启DMA，开了也没有用，因为请求的DAC1信号没有产生，然后DAC1信号又被定时器触发信号控制
	HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 0x07, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn); 
	
	tim7_handle.Instance=TIM7;//生成10MHZ的触发信号，实测触发信号不得高于这个太多，25MHZ就很大了
	tim7_handle.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_ENABLE;
	tim7_handle.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
	tim7_handle.Init.CounterMode=TIM_COUNTERMODE_UP;
	tim7_handle.Init.Period=20-1;
	tim7_handle.Init.Prescaler=1-1;
	tim7_handle.Init.RepetitionCounter=0;
	HAL_TIM_Base_Init(&tim7_handle);
	
	TIM_MasterConfigTypeDef sMasterConfig;
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization(&tim7_handle, &sMasterConfig);

	HAL_TIM_Base_Start(&tim7_handle);
}

void DMA1_Stream7_IRQHandler(void)
{
	if(DMA1->HISR&(0x01<<27))
	{
		DMA1->HIFCR|=(0x01<<27);
		if(!DAC_CH12_USE_BUF_INDEX)
		{
			DAC_CH12_USE_BUF_INDEX=1;
			DMA1_Stream7->CR&=~(0x01<<0);
			DMA1_Stream7->M0AR=(uint32_t)DAC_CH12_DMA_BUF[1];
			DMA1_Stream7->PAR=(uint32_t)&DAC1->DHR12RD;
			DMA1_Stream7->NDTR=DAC_CH12_DMA_BUF_SIZE;
			DMA1_Stream7->CR|=(0x01<<0);
		}
		else if(DAC_CH12_USE_BUF_INDEX)
		{
			DAC_CH12_USE_BUF_INDEX=0;
			DMA1_Stream7->CR&=~(0x01<<0);
			DMA1_Stream7->M0AR=(uint32_t)DAC_CH12_DMA_BUF[0];
			DMA1_Stream7->PAR=(uint32_t)&DAC1->DHR12RD;
			DMA1_Stream7->NDTR=DAC_CH12_DMA_BUF_SIZE;
			DMA1_Stream7->CR|=(0x01<<0);
		}
	}
}
