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
__attribute__((section (".RAM_D2")))static DMA_HandleTypeDef   shdma_spi_tx;
__attribute__((section (".RAM_D2")))static DMA_HandleTypeDef   shdma_spi_rx;
__attribute__((section (".RAM_D2")))static volatile uint8_t    spi_compflg;
__attribute__((section (".RAM_D2")))static uint8_t             spi_dummybuf[4096];


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
	spi_handle.Init.BaudRatePrescaler=SPI_BAUDRATEPRESCALER_32;
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
	spi_handle.Init.MasterKeepIOState=SPI_MASTER_KEEP_IO_STATE_ENABLE;
	spi_handle.Init.IOSwap=SPI_IO_SWAP_DISABLE;
	HAL_SPI_Init(&spi_handle);
	
	memset(&shdma_spi_tx,0,sizeof(DMA_HandleTypeDef));
	shdma_spi_tx.Instance=DMA1_Stream4;
	shdma_spi_tx.Init.Direction=DMA_MEMORY_TO_PERIPH;
	shdma_spi_tx.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
	shdma_spi_tx.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	shdma_spi_tx.Init.MemBurst=DMA_MBURST_SINGLE;
	shdma_spi_tx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
	shdma_spi_tx.Init.MemInc=DMA_MINC_ENABLE;
	shdma_spi_tx.Init.Mode=DMA_NORMAL;
	shdma_spi_tx.Init.PeriphBurst=DMA_PBURST_SINGLE;
	shdma_spi_tx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
	shdma_spi_tx.Init.PeriphInc=DMA_PINC_DISABLE;
	shdma_spi_tx.Init.Priority=DMA_PRIORITY_MEDIUM;
	shdma_spi_tx.Init.Request=DMA_REQUEST_SPI1_TX;
	HAL_DMA_Init(&shdma_spi_tx);

	memset(&shdma_spi_rx,0,sizeof(DMA_HandleTypeDef));
	shdma_spi_rx.Instance=DMA1_Stream5;
	shdma_spi_rx.Init.Direction=DMA_PERIPH_TO_MEMORY;
	shdma_spi_rx.Init.FIFOMode=DMA_FIFOMODE_DISABLE;
	shdma_spi_rx.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	shdma_spi_rx.Init.MemBurst=DMA_MBURST_SINGLE;
	shdma_spi_rx.Init.MemDataAlignment=DMA_MDATAALIGN_BYTE;
	shdma_spi_rx.Init.MemInc=DMA_MINC_ENABLE;
	shdma_spi_rx.Init.Mode=DMA_NORMAL;
	shdma_spi_rx.Init.PeriphBurst=DMA_PBURST_SINGLE;
	shdma_spi_rx.Init.PeriphDataAlignment=DMA_PDATAALIGN_BYTE;
	shdma_spi_rx.Init.PeriphInc=DMA_PINC_DISABLE;
	shdma_spi_rx.Init.Priority=DMA_PRIORITY_MEDIUM;
	shdma_spi_rx.Init.Request=DMA_REQUEST_SPI1_RX;
	HAL_DMA_Init(&shdma_spi_rx);
	
	__HAL_LINKDMA(&spi_handle,hdmatx,shdma_spi_tx);
  __HAL_LINKDMA(&spi_handle,hdmarx,shdma_spi_rx);
  
	HAL_NVIC_SetPriority(SPI1_IRQn,10,0);
	HAL_NVIC_EnableIRQ(SPI1_IRQn);
	HAL_NVIC_SetPriority(DMA1_Stream4_IRQn,11,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn,12,0);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
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
	spi_compflg=0;
	HAL_SPI_TransmitReceive_DMA(&spi_handle,wbuf,rbuf,size);
	while(!spi_compflg);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if(hspi==&spi_handle) spi_compflg=1;
}

void SPI1_IRQHandler(void)
{
	HAL_SPI_IRQHandler(&spi_handle);
}

void DMA1_Stream4_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&shdma_spi_tx);
}

void DMA1_Stream5_IRQHandler(void)
{
	HAL_DMA_IRQHandler(&shdma_spi_rx);
}

/* ----------------------------------------以下是操作SPI时序的操作层------------------------------------- */
static void spi_flash_w25q128_wenable(void)
{
	spi_dummybuf[0]=0x06;
	
	spi_flash_cs_low();
	spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,1);
	spi_flash_cs_high();
}

static void spi_flash_w25q128_waitbusy(void)
{
	while(1)
	{
		spi_dummybuf[0]=0x05;
		spi_dummybuf[1]=0xFF;
		
		spi_flash_cs_low();
	  spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,2);
	  spi_flash_cs_high();
		
		if(!(spi_dummybuf[1]&(0x01<<0))) return;
	}
}

uint32_t spi_flash_w25q128_read_id(void)
{
	spi_flash_w25q128_waitbusy();
	
	spi_dummybuf[0]=0x9F;
	spi_dummybuf[1]=0xFF;
	spi_dummybuf[2]=0xFF;
	spi_dummybuf[3]=0xFF;
	
	spi_flash_cs_low();
  spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,4);
  spi_flash_cs_high();
	return (uint32_t)(((spi_dummybuf[2]<<8)|spi_dummybuf[3])|(spi_dummybuf[1]<<16));
}

void spi_flash_erase_sector_4k(uint32_t addr)
{
	spi_flash_w25q128_waitbusy();
	
	spi_flash_w25q128_wenable();
	
	spi_dummybuf[0]=0x20;
	spi_dummybuf[1]=((addr>>16)&0xff);
	spi_dummybuf[2]=((addr>>8)&0xff);
	spi_dummybuf[3]=((addr>>0)&0xff);
	
	spi_flash_cs_low();
  spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,4);
  spi_flash_cs_high();
}

void spi_flash_erase_sector_32k(uint32_t addr)
{
	spi_flash_w25q128_waitbusy();
	
	spi_flash_w25q128_wenable();
	
	spi_dummybuf[0]=0x52;
	spi_dummybuf[1]=((addr>>16)&0xff);
	spi_dummybuf[2]=((addr>>8)&0xff);
	spi_dummybuf[3]=((addr>>0)&0xff);
	
	spi_flash_cs_low();
  spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,4);
  spi_flash_cs_high();
}

void spi_flash_erase_sector_64k(uint32_t addr)
{
	spi_flash_w25q128_waitbusy();
	
	spi_flash_w25q128_wenable();
	
	spi_dummybuf[0]=0xD8;
	spi_dummybuf[1]=((addr>>16)&0xff);
	spi_dummybuf[2]=((addr>>8)&0xff);
	spi_dummybuf[3]=((addr>>0)&0xff);
	
	spi_flash_cs_low();
  spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,4);
  spi_flash_cs_high();
}

void spi_flash_erase_chip(void)
{
	spi_flash_w25q128_waitbusy();
	
	spi_flash_w25q128_wenable();
	
	spi_dummybuf[0]=0xC7;
	
	spi_flash_cs_low();
  spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,1);
  spi_flash_cs_high();
}

void spi_flash_read(uint32_t addr,uint8_t* data,uint32_t len)
{
	uint32_t remain=len;
	uint16_t chunk;

	spi_flash_w25q128_waitbusy();

	spi_dummybuf[0]=0x03;
	spi_dummybuf[1]=(addr>>16)&0xff;
	spi_dummybuf[2]=(addr>>8)&0xff;
	spi_dummybuf[3]=addr&0xff;

	spi_flash_cs_low();
	spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,4);//命令写无所谓
	while(remain)
	{
		chunk=remain>4096?4096:remain;//分片4096是受限于spi_dummybuf大小，因为读写不能同一buffer
		spi_flash_tranmit_recv(spi_dummybuf,data,chunk);//上层缓冲区读写绝不能同一buffer,否则buffer被破坏
		data+=chunk;
		remain-=chunk;
	}
	spi_flash_cs_high();
}

static void spi_flash_page_write(uint32_t addr,uint8_t* data,uint16_t len)
{
	spi_flash_w25q128_waitbusy();
	
	spi_flash_w25q128_wenable();
	
	spi_dummybuf[0]=0x02;
	spi_dummybuf[1]=(addr>>16)&0xff;
	spi_dummybuf[2]=(addr>>8)&0xff;
	spi_dummybuf[3]=addr&0xff;
	
	spi_flash_cs_low();
	spi_flash_tranmit_recv(spi_dummybuf,spi_dummybuf,4);//命令写无所谓
  spi_flash_tranmit_recv(data,spi_dummybuf,len);//上层缓冲区读写绝不能同一buffer,页的话256字节不用分片够的
  spi_flash_cs_high();
}

void spi_flash_write(uint32_t addr,uint8_t* data,uint32_t len)
{
	uint32_t page_remain;
  uint32_t write_len;
	while(len > 0)
	{
			page_remain = 256 - (addr & 255);
			write_len = (len < page_remain) ? len : page_remain;
			spi_flash_page_write(addr, data, write_len);
			addr += write_len;
			data += write_len;
			len  -= write_len;
	}
}
