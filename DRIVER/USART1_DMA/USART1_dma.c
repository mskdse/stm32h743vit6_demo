#include "usart1_dma.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define USART1_DMA_TX_SIZE          1024
__attribute__((section (".RAM_D2")))static volatile uint8_t USART1_DMA_TX_COMPTLE;
__attribute__((section(".RAM_D2"))) 
__attribute__((aligned(32))) 
static uint8_t USART1_DMA_TX_FIFO[USART1_DMA_TX_SIZE];

void usart1_dma_init(uint32_t badue)
{
	__HAL_RCC_DMA1_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_USART1_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_init_structure;
	gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
	gpio_init_structure.Pull      = GPIO_NOPULL;
	gpio_init_structure.Speed     = GPIO_SPEED_FREQ_HIGH;
	gpio_init_structure.Alternate = GPIO_AF7_USART1;
	gpio_init_structure.Pin       = GPIO_PIN_9|GPIO_PIN_10;
	HAL_GPIO_Init(GPIOA, &gpio_init_structure);
	
	UART_HandleTypeDef huart;
	memset(&huart,0,sizeof(UART_HandleTypeDef));
	huart.Instance=USART1;
  huart.Init.BaudRate=badue;
	huart.Init.ClockPrescaler=USART_PRESCALER_DIV1;
	huart.Init.Mode=USART_MODE_TX_RX;
	huart.Init.Parity=USART_PARITY_NONE;
	huart.Init.StopBits=USART_STOPBITS_1;
	huart.Init.WordLength=USART_WORDLENGTH_8B;
	huart.Init.HwFlowCtl=UART_HWCONTROL_NONE;
	huart.Init.OverSampling=UART_OVERSAMPLING_16;
	huart.Init.OneBitSampling=UART_ONE_BIT_SAMPLE_DISABLE;
	HAL_UART_Init(&huart);
	
	DMA_HandleTypeDef hdma_usart_tx;
	memset(&hdma_usart_tx,0,sizeof(DMA_HandleTypeDef));
	hdma_usart_tx.Instance=DMA1_Stream2;
	hdma_usart_tx.Init.Direction=DMA_MEMORY_TO_PERIPH;
	hdma_usart_tx.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
	hdma_usart_tx.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	hdma_usart_tx.Init.MemBurst=DMA_MBURST_SINGLE;
	hdma_usart_tx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
	hdma_usart_tx.Init.MemInc=DMA_MINC_ENABLE;
	hdma_usart_tx.Init.Mode=DMA_NORMAL;
	hdma_usart_tx.Init.PeriphBurst=DMA_PBURST_SINGLE;
	hdma_usart_tx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
	hdma_usart_tx.Init.PeriphInc=DMA_PINC_DISABLE;
	hdma_usart_tx.Init.Priority=DMA_PRIORITY_MEDIUM;
	hdma_usart_tx.Init.Request=DMA_REQUEST_USART1_TX;
	HAL_DMA_Init(&hdma_usart_tx);
	
	USART1_DMA_TX_COMPTLE=1;//放行第一次发送
	USART1->ICR=0xFF;//清空全部标志位
	USART1->CR3|=(0x01<<7);//使能串口发送DMA
	DMA1_Stream2->CR|=(0x01<<4);//使能发送完成中断
	DMA1_Stream2->CR&=~(0x01<<0);//关闭发送DMA，按需开启
	
	HAL_NVIC_SetPriority(DMA1_Stream2_IRQn,14,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
}

void usart1_my_printf(const char *format, ...)
{
	va_list args;
	int len;
	
	while(!USART1_DMA_TX_COMPTLE);
	USART1_DMA_TX_COMPTLE=0;
	
	va_start(args, format);
	len=vsnprintf((char *)USART1_DMA_TX_FIFO,USART1_DMA_TX_SIZE,format,args);
	va_end(args);
	USART1_DMA_TX_FIFO[len]='\0';
	
	DMA1_Stream2->NDTR=len;
	DMA1_Stream2->M0AR=(uint32_t)USART1_DMA_TX_FIFO;
	DMA1_Stream2->PAR=(uint32_t)&USART1->TDR;
  DMA1_Stream2->CR|=(0x01<<0);
}

void DMA1_Stream2_IRQHandler(void)
{
	if(DMA1->LISR&(0x01<<21))
	{
		DMA1->LIFCR|=(0x01<<21);
		DMA1_Stream2->CR&=~(0x01<<0);
		USART1_DMA_TX_COMPTLE=1;
	}
}
