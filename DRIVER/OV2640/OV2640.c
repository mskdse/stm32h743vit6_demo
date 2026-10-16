#include "OV2640.h"
#include "drvp_fmc_lcd.h"
#include <string.h>

__attribute__((section (".RAM_D2")))static I2C_HandleTypeDef  ov2640_sccb_i2c;

/*
  D0-PC6
   1-PC7
   2-PC8
   3-PC9
   4-PC11
   5-PD3
   6-PB8
   7-PB9

  CLK-PA6
  HSYNC-PA4
  VSYNC-PB7
   
  SCL-PB10
  SDA-PB11
  
  RST-PB15  //普通GPIO
  PWDN-PB14 //普通GPIO
*/

static void ov2640_board_init(void)
{
	__HAL_RCC_DCMI_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_I2C2_CLK_ENABLE();
	
	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Alternate=GPIO_AF4_I2C2;
	gpio_cfg.Mode=GPIO_MODE_AF_OD;
	gpio_cfg.Pin=GPIO_PIN_10|GPIO_PIN_11;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
	memset(&ov2640_sccb_i2c,0,sizeof(I2C_HandleTypeDef));
	ov2640_sccb_i2c.Instance=I2C2;
	ov2640_sccb_i2c.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
	ov2640_sccb_i2c.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE;
	ov2640_sccb_i2c.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE;
	ov2640_sccb_i2c.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
	ov2640_sccb_i2c.Init.OwnAddress1=0x00;
	ov2640_sccb_i2c.Init.OwnAddress2=0x00;
	ov2640_sccb_i2c.Init.OwnAddress2Masks=I2C_OA2_NOMASK;
	ov2640_sccb_i2c.Init.Timing=0xB0420F13;
	HAL_I2C_Init(&ov2640_sccb_i2c);
	
	gpio_cfg.Alternate=GPIO_AF13_DCMI;
	gpio_cfg.Mode=GPIO_MODE_AF_PP;
	gpio_cfg.Pull=GPIO_NOPULL;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_VERY_HIGH;
	
	gpio_cfg.Pin=GPIO_PIN_6;
	HAL_GPIO_Init(GPIOC,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_7;
	HAL_GPIO_Init(GPIOC,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_8;
	HAL_GPIO_Init(GPIOC,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_9;
	HAL_GPIO_Init(GPIOC,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_11;
	HAL_GPIO_Init(GPIOC,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_3;
	HAL_GPIO_Init(GPIOD,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_8;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_9;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_6;
	HAL_GPIO_Init(GPIOA,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_4;
	HAL_GPIO_Init(GPIOA,&gpio_cfg);
	
	gpio_cfg.Pin=GPIO_PIN_7;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	
  memset(&gpio_cfg,0,sizeof(GPIO_InitTypeDef));
	gpio_cfg.Mode=GPIO_MODE_OUTPUT_PP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	gpio_cfg.Pin=GPIO_PIN_14;
	gpio_cfg.Pull=GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_14,GPIO_PIN_RESET);
	gpio_cfg.Pin=GPIO_PIN_15;
	gpio_cfg.Pull=GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,GPIO_PIN_SET);
	for(int i=0;i<100;i++) __nop();
}

static void ov2640_sccb_wReg(uint8_t reg,uint8_t val)
{
	HAL_I2C_Mem_Write(&ov2640_sccb_i2c,OV2640_DEVADDR_8BIT,reg,I2C_MEMADD_SIZE_8BIT,&val,1,HAL_MAX_DELAY);
}

static void ov2640_sccb_rReg(uint8_t reg,uint8_t* val)
{
	HAL_I2C_Mem_Read(&ov2640_sccb_i2c,OV2640_DEVADDR_8BIT,reg,I2C_MEMADD_SIZE_8BIT,val,1,HAL_MAX_DELAY);
}

uint16_t ov2640_read_pid(void)
{
	uint8_t pid[2];
	ov2640_sccb_wReg(0xFF,0x01);
	ov2640_sccb_rReg(0x0A,&pid[0]);
	ov2640_sccb_rReg(0x0B,&pid[1]);
	return (uint16_t)((pid[0]<<8)|pid[1]);
}

void ov2640_init(void)
{
	ov2640_board_init();
	
	DCMI->CR&=~(0x01<<14);//失能DCMI
	DCMI->CR&=~(0x01<<0);//失能DCMI捕获
	
	DCMI->CR&=~(0x01<<19);//捕获所有行
	DCMI->CR&=~(0x01<<17);//捕获所有数据
	DCMI->CR&=~(0x01<<16);
	DCMI->CR&=~(0x01<<11);//每个时钟捕获8位
	DCMI->CR&=~(0x01<<10);
	DCMI->CR&=~(0x01<<9);//捕获所有帧
	DCMI->CR&=~(0x01<<8);
	DCMI->CR&=~(0x01<<7);//VSYNC极性
	DCMI->CR&=~(0x01<<6);//HSYNC极性
	DCMI->CR&=~(0x01<<5);//PLCK时钟极性
	DCMI->CR&=~(0x01<<3);//正常RGB格式，而非GPEG格式
	DCMI->CR&=~(0x01<<2);//捕获所有图像，不经过裁剪
	DCMI->CR&=~(0x01<<1);//连续捕获模式
	DCMI->IER|=(0x01<<1);//开启捕获完成中断
	
	DCMI->CR|=(0x01<<14);//使能DCMI
	
	/* 下面是DMA的配置，一部分配置单独给出API使用寄存器方式 */
	DMA_HandleTypeDef hdma;
	hdma.Instance=DMA1_Stream4;
	hdma.Init.Direction=DMA_PERIPH_TO_MEMORY;
	hdma.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
	hdma.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	hdma.Init.MemBurst=DMA_MBURST_SINGLE;
	hdma.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;//FMC模拟8080支持16位
	hdma.Init.MemInc=DMA_MINC_DISABLE;
	hdma.Init.Mode=DMA_CIRCULAR;//循环模式配合连续采集
	hdma.Init.PeriphBurst=DMA_PBURST_SINGLE;
	hdma.Init.PeriphDataAlignment=DMA_PDATAALIGN_WORD;//DCMI->DR是32位
	hdma.Init.PeriphInc=DMA_PINC_DISABLE;
	hdma.Init.Priority=DMA_PRIORITY_VERY_HIGH;
	hdma.Init.Request=DMA_REQUEST_DCMI;
	HAL_DMA_Init(&hdma);
	
	HAL_NVIC_SetPriority(DCMI_IRQn, 0x09, 0);
  HAL_NVIC_EnableIRQ(DCMI_IRQn); 
}

void ov2640_dma_transfer_start(void)
{
	DMA1_Stream4->CR|=(0x01<<4);//传输完成中断
	DMA1_Stream4->NDTR=1;//每次传输一个，配合循环模式刷全图，之所以不直接240*320，是因为DMA一次最大65536
	HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0x08, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn); 
	
	DCMI->CR|=(0x01<<0);//使能DCMI捕获
	DMA1_Stream4->CR|=(0x01<<0);//开启DMA
}

void DCMI_IRQHandler(void)
{
	if(DCMI->SR&(0x01<<0))
	{
		DCMI->ICR|=(0x01<<0);
	}
}
