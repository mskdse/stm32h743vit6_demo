#include "spi_flash.h"
#include <string.h>
#include <stdbool.h>

/*
	 spi-cs  -----  PD3
	 spi-clk -----  PB3
	 spi-mosi-----  PB5
	 spi_miso-----  PB4
*/

__attribute__((section (".RAM_D2")))static SPI_HandleTypeDef   spi_handle;
__attribute__((section (".RAM_D2")))static volatile uint8_t    spi_tx_compflg;
__attribute__((section (".RAM_D2")))static volatile uint8_t    spi_rx_compflg;
__attribute__((section (".RAM_D2")))static uint8_t             spi_cmdbuf[8];

/* SPI-W25Q128初始化 */
void spi_flash_w25q128_init(void)
{
	__HAL_RCC_SPI1_FORCE_RESET();
	__HAL_RCC_SPI1_RELEASE_RESET();
	__HAL_RCC_SPI1_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pull=GPIO_NOPULL;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_cfg.Alternate=GPIO_AF5_SPI1;
	gpio_cfg.Pin=GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	memset(&gpio_cfg,0,sizeof(GPIO_InitTypeDef));
	gpio_cfg.Pin=GPIO_PIN_3;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Mode=GPIO_MODE_OUTPUT_PP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOD,&gpio_cfg);
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,GPIO_PIN_SET);
	
	/* 初始化SPI器件 */
	memset(&spi_handle,0,sizeof(SPI_HandleTypeDef));
	spi_handle.Instance=SPI1;
	spi_handle.Init.Mode=SPI_MODE_MASTER;
	spi_handle.Init.Direction=SPI_DIRECTION_2LINES;
	spi_handle.Init.DataSize=SPI_DATASIZE_8BIT;
	spi_handle.Init.CLKPolarity=SPI_POLARITY_LOW;
	spi_handle.Init.CLKPhase=SPI_PHASE_1EDGE;
	spi_handle.Init.NSS=SPI_NSS_SOFT;
	spi_handle.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_2;
	spi_handle.Init.FirstBit=SPI_FIRSTBIT_MSB;
	spi_handle.Init.TIMode=SPI_TIMODE_DISABLE;
	spi_handle.Init.CRCCalculation=SPI_CRCCALCULATION_DISABLE;
	spi_handle.Init.CRCPolynomial=0;
	spi_handle.Init.CRCLength=SPI_CRC_LENGTH_DATASIZE;
	spi_handle.Init.NSSPMode=SPI_NSS_PULSE_DISABLE;
	spi_handle.Init.NSSPolarity=SPI_NSS_POLARITY_LOW;
	spi_handle.Init.FifoThreshold=SPI_FIFO_THRESHOLD_01DATA;
	spi_handle.Init.TxCRCInitializationPattern=SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	spi_handle.Init.RxCRCInitializationPattern=SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	spi_handle.Init.MasterSSIdleness=SPI_MASTER_SS_IDLENESS_00CYCLE;
	spi_handle.Init.MasterInterDataIdleness=SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	spi_handle.Init.MasterReceiverAutoSusp=SPI_MASTER_RX_AUTOSUSP_DISABLE;
	spi_handle.Init.MasterKeepIOState=SPI_MASTER_KEEP_IO_STATE_DISABLE;
	spi_handle.Init.IOSwap=SPI_IO_SWAP_DISABLE;
	HAL_SPI_Init(&spi_handle);
	
	DMA_HandleTypeDef hdma_spi_tx;
	memset(&hdma_spi_tx,0,sizeof(DMA_HandleTypeDef));
	hdma_spi_tx.Instance=DMA1_Stream4;
	hdma_spi_tx.Init.Direction=DMA_MEMORY_TO_PERIPH;
	hdma_spi_tx.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
	hdma_spi_tx.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	hdma_spi_tx.Init.MemBurst=DMA_MBURST_SINGLE;
	hdma_spi_tx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
	hdma_spi_tx.Init.MemInc=DMA_MINC_ENABLE;
	hdma_spi_tx.Init.Mode=DMA_NORMAL;
	hdma_spi_tx.Init.PeriphBurst=DMA_PBURST_SINGLE;
	hdma_spi_tx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
	hdma_spi_tx.Init.PeriphInc=DMA_PINC_DISABLE;
	hdma_spi_tx.Init.Priority=DMA_PRIORITY_MEDIUM;
	hdma_spi_tx.Init.Request=DMA_REQUEST_SPI1_TX;
	HAL_DMA_Init(&hdma_spi_tx);
	
	DMA_HandleTypeDef hdma_spi_rx;
	memset(&hdma_spi_rx,0,sizeof(DMA_HandleTypeDef));
	hdma_spi_rx.Instance=DMA1_Stream5;
	hdma_spi_rx.Init.Direction=DMA_PERIPH_TO_MEMORY;
	hdma_spi_rx.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
	hdma_spi_rx.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	hdma_spi_rx.Init.MemBurst=DMA_MBURST_SINGLE;
	hdma_spi_rx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
	hdma_spi_rx.Init.MemInc=DMA_MINC_ENABLE;
	hdma_spi_rx.Init.Mode=DMA_NORMAL;
	hdma_spi_rx.Init.PeriphBurst=DMA_PBURST_SINGLE;
	hdma_spi_rx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
	hdma_spi_rx.Init.PeriphInc=DMA_PINC_DISABLE;
	hdma_spi_rx.Init.Priority=DMA_PRIORITY_MEDIUM;
	hdma_spi_rx.Init.Request=DMA_REQUEST_SPI1_RX;
	HAL_DMA_Init(&hdma_spi_rx);
	
	SPI1->CFG1|=(0x01<<14);//使能SPI的收发DMA
	SPI1->CFG1|=(0x01<<15);
	SPI1->CR1|=(0x01<<0);//使能SPI
	
	DMA1->HIFCR|=(0x01<<5);//提前清空全满中断标志位
	DMA1->HIFCR|=(0x01<<11);
	DMA1_Stream4->CR|=(0x01<<4);//开启全满中断
	DMA1_Stream5->CR|=(0x01<<4);
	HAL_NVIC_SetPriority(DMA1_Stream4_IRQn,11,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn,12,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
	
	spi_tx_compflg=0;
	spi_rx_compflg=0;
	DMA1_Stream4->CR&=~(0x01<<0);//失能DMA
	DMA1_Stream5->CR&=~(0x01<<0);//失能DMA
}

static inline void spi_flash_cs_low(void)
{
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,GPIO_PIN_RESET);
}

static inline void spi_flash_cs_high(void)
{
	HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,GPIO_PIN_SET);
}

static void spi_flash_tranmit_recv(uint8_t* wbuf,uint8_t* rbuf,uint16_t size)
{
	DMA1_Stream4->M0AR=(uint32_t)wbuf;
	DMA1_Stream4->PAR=(uint32_t)&SPI1->TXDR;
	DMA1_Stream4->NDTR=size;
	DMA1_Stream5->M0AR=(uint32_t)rbuf;
	DMA1_Stream5->PAR=(uint32_t)&SPI1->RXDR;
	DMA1_Stream5->NDTR=size;
	
	DMA1_Stream5->CR|=(0x01<<0);//使能接收DMA
	SPI1->CR2=size;//设置数据流大小
	SPI1->CR1|=(0x01<<9);//开启主传输开始标志位，该位硬件清空
	DMA1_Stream4->CR|=(0x01<<0);//使能发送DMA
	
	while(!spi_tx_compflg||!spi_rx_compflg);
	while(!(SPI1->SR&(0x01<<3)));//手册说要等待EOT位
	SPI1->IFCR|=(0x01<<3);//清空EOT位
	spi_tx_compflg=0;
	spi_rx_compflg=0;
	DMA1_Stream4->CR&=~(0x01<<0);//EOT位置位后再失能DMA，这里不能在中断里面失能
	DMA1_Stream5->CR&=~(0x01<<0);
}

void DMA1_Stream4_IRQHandler(void)
{
	if(DMA1->HISR&(0x01<<5))
	{
		DMA1->HIFCR|=(0x01<<5);
		spi_tx_compflg=1;
	}
}

void DMA1_Stream5_IRQHandler(void)
{
	if(DMA1->HISR&(0x01<<11))
	{
		DMA1->HIFCR|=(0x01<<11);
		spi_rx_compflg=1;
	}
}

/* ----------------------------------------以下是操作SPI时序的操作层------------------------------------- */
static void spi_flash_w25q128_wenable(void)
{
	spi_cmdbuf[0]=0x06;
	spi_flash_cs_low();
	spi_flash_tranmit_recv(spi_cmdbuf,spi_cmdbuf,1);
	spi_flash_cs_high();
}

static void spi_flash_w25q128_waitbusy(void)
{
	while(1)
	{
		spi_cmdbuf[0]=0x05;
		spi_cmdbuf[1]=0xFF;
		spi_flash_cs_low();
	  spi_flash_tranmit_recv(spi_cmdbuf,spi_cmdbuf,2);
	  spi_flash_cs_high();
		if(!(spi_cmdbuf[1]&(0x01<<0))) return;
	}
}

uint8_t spi_flash_w25q128_readid(void)
{
	spi_cmdbuf[0]=0xAB;
	spi_cmdbuf[1]=0xFF;
	spi_cmdbuf[2]=0xFF;
	spi_cmdbuf[3]=0xFF;
	spi_cmdbuf[4]=0xFF;
	spi_flash_w25q128_waitbusy();
	spi_flash_cs_low();
  spi_flash_tranmit_recv(spi_cmdbuf,spi_cmdbuf,5);
  spi_flash_cs_high();
	return spi_cmdbuf[4];
}
