#include "wm8978.h"
#include <string.h>

__attribute__((section (".RAM_D2")))static I2C_HandleTypeDef  wm8978_i2c;
__attribute__((section (".RAM_D2")))static I2S_HandleTypeDef  hi2s2;

#define I2S2_TX_BUF_SIZE  1024
__attribute__((section (".RAM_D2")))static uint8_t I2S2_TX_BUFFER[2][I2S2_TX_BUF_SIZE]; 

static void wm8978_i2c_wReg(uint8_t reg,uint8_t val)
{
	HAL_I2C_Mem_Write(&wm8978_i2c,WM8978_DEVADDR_8BIT,reg,I2C_MEMADD_SIZE_8BIT,&val,1,HAL_MAX_DELAY);
}

static void wm8978_i2c_rReg(uint8_t reg,uint8_t* val)
{
	HAL_I2C_Mem_Read(&wm8978_i2c,WM8978_DEVADDR_8BIT,reg,I2C_MEMADD_SIZE_8BIT,val,1,HAL_MAX_DELAY);
}

static void wm8978_borad_init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	
	memset(&wm8978_i2c,0,sizeof(I2C_HandleTypeDef));
	memset(&hi2s2,0,sizeof(I2S_HandleTypeDef));
	
	__HAL_RCC_SPI2_CLK_ENABLE();
	__HAL_RCC_DMA2_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_I2C2_CLK_ENABLE();
	
	GPIO_InitStruct.Alternate=GPIO_AF5_SPI2;
	GPIO_InitStruct.Mode=GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pin=GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
	GPIO_InitStruct.Pull=GPIO_NOPULL;
	GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOB,&GPIO_InitStruct);
	GPIO_InitStruct.Pin=GPIO_PIN_6;
	HAL_GPIO_Init(GPIOC,&GPIO_InitStruct);
	
	GPIO_InitStruct.Alternate=GPIO_AF4_I2C2;
	GPIO_InitStruct.Mode=GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pin=GPIO_PIN_10|GPIO_PIN_11;
	GPIO_InitStruct.Pull=GPIO_PULLUP;
	GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB,&GPIO_InitStruct);
	
	wm8978_i2c.Instance=I2C2;
	wm8978_i2c.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
	wm8978_i2c.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE;
	wm8978_i2c.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE;
	wm8978_i2c.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
	wm8978_i2c.Init.OwnAddress1=0x00;
	wm8978_i2c.Init.OwnAddress2=0x00;
	wm8978_i2c.Init.OwnAddress2Masks=I2C_OA2_NOMASK;
	wm8978_i2c.Init.Timing=0xB0420F13;
	HAL_I2C_Init(&wm8978_i2c);
}

void wm8978_init(void)
{
	wm8978_borad_init();

	HAL_I2S_DeInit(&hi2s2);
	hi2s2.Instance=SPI2;
	hi2s2.Init.AudioFreq=I2S_AUDIOFREQ_44K;//采样频率44.1KHZ
	hi2s2.Init.CPOL=I2S_CPOL_LOW;//I2S时钟极性为低电平时可修改数据，上升沿采样
	hi2s2.Init.Data24BitAlignment=I2S_DATA_24BIT_ALIGNMENT_RIGHT;//数据格式右对齐
	hi2s2.Init.DataFormat=I2S_DATAFORMAT_16B;//单个声道数据长度是16BIT
	hi2s2.Init.FirstBit=I2S_FIRSTBIT_MSB;//数据格式高位在前
	hi2s2.Init.MasterKeepIOState=I2S_MASTER_KEEP_IO_STATE_DISABLE;//外设在禁止的时候不得操作GPIO
	hi2s2.Init.MCLKOutput=I2S_MCLKOUTPUT_ENABLE;//使能主时钟输出
	hi2s2.Init.Mode=I2S_MODE_MASTER_TX;//主模式仅发送，因为我只需要播放音乐，不需要录音
	hi2s2.Init.Standard=I2S_STANDARD_PHILIPS;//I2S标准
	hi2s2.Init.WSInversion=I2S_WS_INVERSION_DISABLE;//左右声道不反转，默认WS低电平是传左声道数据，高是右
	HAL_I2S_Init(&hi2s2);
	
	DMA_HandleTypeDef  i2s2_tx_dma;
	i2s2_tx_dma.Instance=DMA2_Stream1;
	i2s2_tx_dma.Init.Direction=DMA_MEMORY_TO_PERIPH;
	i2s2_tx_dma.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
	i2s2_tx_dma.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	i2s2_tx_dma.Init.MemBurst=DMA_MBURST_SINGLE;
	i2s2_tx_dma.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;//数据格式16位
	i2s2_tx_dma.Init.MemInc=DMA_MINC_ENABLE;
	i2s2_tx_dma.Init.Mode=DMA_CIRCULAR;//循环模式配合连续采集
	i2s2_tx_dma.Init.PeriphBurst=DMA_PBURST_SINGLE;
	i2s2_tx_dma.Init.PeriphDataAlignment=DMA_PDATAALIGN_HALFWORD;//数据格式16位
	i2s2_tx_dma.Init.PeriphInc=DMA_PINC_DISABLE;
	i2s2_tx_dma.Init.Priority=DMA_PRIORITY_VERY_HIGH;
	i2s2_tx_dma.Init.Request=DMA_REQUEST_SPI2_TX;
	HAL_DMA_Init(&i2s2_tx_dma);
}
