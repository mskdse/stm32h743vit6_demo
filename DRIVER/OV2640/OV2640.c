#include "OV2640.h"
#include "drvp_fmc_lcd.h"
#include <string.h>
#include "usart1_dma.h"

__attribute__((section (".RAM_D2")))static I2C_HandleTypeDef  ov2640_sccb_i2c;

#define CAM_WIDTH       DRVP_LCD_LENGTH
#define CAM_HEIGHT      DRVP_LCD_WIGTH
#define CAM_DMA_WORDS   (CAM_WIDTH*CAM_HEIGHT*2/4) //RGB565=2字节，长*宽*2字节后除以一个字（4字节）大小为最终的帧缓冲区
__attribute__((section(".RAM_D1"), aligned(32)))static uint32_t ov2640_frame_buf[CAM_DMA_WORDS];

__attribute__((section (".RAM_D2")))static volatile uint8_t  ov2640_frame_tranflg;

/* UXGA 模式，15帧，1600x1200 */
const unsigned char OV2640_UXGA[][2]=
{
  0xff, 0x00,
  0x2c, 0xff,
  0x2e, 0xdf,
  0xff, 0x01,
  0x3c, 0x32,
  0x11, 0x00,
  0x09, 0x02,
  0x04, 0xD8,	//水平垂直翻转
  0x13, 0xe5,
  0x14, 0x48,
  0x2c, 0x0c,
  0x33, 0x78,
  0x3a, 0x33,
  0x3b, 0xfB,
  0x3e, 0x00,
  0x43, 0x11,
  0x16, 0x10,
  0x4a, 0x81,
  0x21, 0x99,
  0x24, 0x40,
  0x25, 0x38,
  0x26, 0x82,
  0x5c, 0x00,
  0x63, 0x00,
  0x46, 0x3f,
  0x0c, 0x3c,
  0x61, 0x70,
  0x62, 0x80,
  0x7c, 0x05,
  0x20, 0x80,
  0x28, 0x30,
  0x6c, 0x00,
  0x6d, 0x80,
  0x6e, 0x00,
  0x70, 0x02,
  0x71, 0x94,
  0x73, 0xc1,
  0x3d, 0x34,
  0x5a, 0x57,
  0x12, 0x00,
  0x11, 0x00,//0x00，15帧 0x80，30帧
  0x17, 0x11,
  0x18, 0x75,
  0x19, 0x01,
  0x1a, 0x97,
  0x32, 0x36,
  0x03, 0x0f,
  0x37, 0x40,
  0x4f, 0xbb,
  0x50, 0x9c,
  0x5a, 0x57,
  0x6d, 0x80,
  0x6d, 0x38,
  0x39, 0x02,
  0x35, 0x88,
  0x22, 0x0a,
  0x37, 0x40,
  0x23, 0x00,
  0x34, 0xa0,
  0x36, 0x1a,
  0x06, 0x02,
  0x07, 0xc0,
  0x0d, 0xb7,
  0x0e, 0x01,
  0x4c, 0x00,
  0xff, 0x00,
  0xe5, 0x7f,
  0xf9, 0xc0,
  0x41, 0x24,
  0xe0, 0x14,
  0x76, 0xff,
  0x33, 0xa0,
  0x42, 0x20,
  0x43, 0x18,
  0x4c, 0x00,
  0x87, 0xd0,
  0x88, 0x3f,
  0xd7, 0x03,
  0xd9, 0x10,
  0xd3, 0x82,
  0xc8, 0x08,
  0xc9, 0x80,
  0x7d, 0x00,
  0x7c, 0x03,
  0x7d, 0x48,
  0x7c, 0x08,
  0x7d, 0x20,
  0x7d, 0x10,
  0x7d, 0x0e,
  0x90, 0x00,
  0x91, 0x0e,
  0x91, 0x1a,
  0x91, 0x31,
  0x91, 0x5a,
  0x91, 0x69,
  0x91, 0x75,
  0x91, 0x7e,
  0x91, 0x88,
  0x91, 0x8f,
  0x91, 0x96,
  0x91, 0xa3,
  0x91, 0xaf,
  0x91, 0xc4,
  0x91, 0xd7,
  0x91, 0xe8,
  0x91, 0x20,
  0x92, 0x00,
  0x93, 0x06,
  0x93, 0xe3,
  0x93, 0x02,
  0x93, 0x02,
  0x93, 0x00,
  0x93, 0x04,
  0x93, 0x00,
  0x93, 0x03,
  0x93, 0x00,
  0x93, 0x00,
  0x93, 0x00,
  0x93, 0x00,
  0x93, 0x00,
  0x93, 0x00,
  0x93, 0x00,
  0x96, 0x00,
  0x97, 0x08,
  0x97, 0x19,
  0x97, 0x02,
  0x97, 0x0c,
  0x97, 0x24,
  0x97, 0x30,
  0x97, 0x28,
  0x97, 0x26,
  0x97, 0x02,
  0x97, 0x98,
  0x97, 0x80,
  0x97, 0x00,
  0x97, 0x00,
  0xc3, 0xef,
  0xff, 0x00,
  0xba, 0xdc,
  0xbb, 0x08,
  0xb6, 0x24,
  0xb8, 0x33,
  0xb7, 0x20,
  0xb9, 0x30,
  0xb3, 0xb4,
  0xb4, 0xca,
  0xb5, 0x43,
  0xb0, 0x5c,
  0xb1, 0x4f,
  0xb2, 0x06,
  0xc7, 0x00,
  0xc6, 0x51,
  0xc5, 0x11,
  0xc4, 0x9c,
  0xbf, 0x00,
  0xbc, 0x64,
  0xa6, 0x00,
  0xa7, 0x1e,
  0xa7, 0x6b,
  0xa7, 0x47,
  0xa7, 0x33,
  0xa7, 0x00,
  0xa7, 0x23,
  0xa7, 0x2e,
  0xa7, 0x85,
  0xa7, 0x42,
  0xa7, 0x33,
  0xa7, 0x00,
  0xa7, 0x23,
  0xa7, 0x1b,
  0xa7, 0x74,
  0xa7, 0x42,
  0xa7, 0x33,
  0xa7, 0x00,
  0xa7, 0x23,
  0xc0, 0xc8,
  0xc1, 0x96,
  0x8c, 0x00,
  0x86, 0x3d,
  0x50, 0x92,
  0x51, 0x90,
  0x52, 0x2c,
  0x53, 0x00,
  0x54, 0x00,
  0x55, 0x88,
  0x5a, 0x50,
  0x5b, 0x3c,
  0x5c, 0x00,
  0xd3, 0x08,
  0x7f, 0x00,
  0xda, 0x00,
  0xe5, 0x1f,
  0xe1, 0x67,
  0xe0, 0x00,
  0xdd, 0x7f,
  0x05, 0x00,
  0xff, 0x00,
  0xe0, 0x04,
  0xc0, 0xc8,
  0xc1, 0x96,
  0x86, 0x3d,
  0x50, 0x92,
  0x51, 0x90,
  0x52, 0x2c,
  0x53, 0x00,
  0x54, 0x00,
  0x55, 0x88,
  0x57, 0x00,
  0x5a, 0x50,
  0x5b, 0x3c,
  0x5c, 0x00,
  0xd3, 0x04,//系统时钟
  0xe0, 0x00,
  0xFF, 0x00,
  0x05, 0x00,
  0xDA, 0x08,
  0xda, 0x09,
  0x98, 0x00,
  0x99, 0x00,
  0x00, 0x00,
	

  
//  0xff, 0x00,
//  0xe0, 0x04,
//  0xc0, 0xc8,
//  0xc1, 0x96,
//  0x86, 0x35,
//  0x50, 0x80,//必须配置这个寄存器控制时钟
//  0x51, 0x90,
//  0x52, 0x2c,
//  0x53, 0x00,
//  0x54, 0x00,
//  0x55, 0x88,
//  0x57, 0x00,
//  0x5a, 0x78,//480
//  0x5b, 0x44,//272
//  0x5c, 0x00,
//  0xd3, 0x04,
//  0xe0, 0x00,

};

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
	__HAL_RCC_DMA1_CLK_ENABLE();
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
	ov2640_frame_tranflg=0;
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

static uint8_t ov2640_put_set(uint16_t width,uint16_t height)
{
	uint16_t outh,outw;
	uint8_t  temp;

	if(width%4)return 1;
	if(height%4)return 2;

	outw=width/4;
	outh=height/4;

	ov2640_sccb_wReg(0xFF,0x00);
	ov2640_sccb_wReg(0xE0,0x04);
	ov2640_sccb_wReg(0x5A,outw&0xFF);
	ov2640_sccb_wReg(0x5B,outh&0xFF);

	temp=(outw>>8)&0x03;
	temp|=(outh>>6)&0x04;
	ov2640_sccb_wReg(0x5C,temp);
	ov2640_sccb_wReg(0xE0,0x00);

	return 0;
}

void ov2640_uxga_config(void)
{
  uint32_t i;
	
	/*1.摄像头复位*/
  /*OV2640有两组寄存器，设置0xFF寄存器的值为0或为1时可选择使用不同组的寄存器*/
  ov2640_sccb_wReg(0xFF, 0x01);
  ov2640_sccb_wReg(0x12, 0x80);

	/*2.进行三次寄存器写入，确保配置写入正常
	(在使用摄像头长排线时，IIC数据线干扰较大，必须多次写入来保证正常)*/
  /* 写入寄存器配置 */
	for(i=0; i<(sizeof(OV2640_UXGA)/2); i++) ov2640_sccb_wReg(OV2640_UXGA[i][0], OV2640_UXGA[i][1]);
  for(i=0; i<(sizeof(OV2640_UXGA)/2); i++) ov2640_sccb_wReg(OV2640_UXGA[i][0], OV2640_UXGA[i][1]);
	for(i=0; i<(sizeof(OV2640_UXGA)/2); i++) ov2640_sccb_wReg(OV2640_UXGA[i][0], OV2640_UXGA[i][1]);
	
	/* 3.设置输出的图像大小*/
	ov2640_put_set(DRVP_LCD_LENGTH,DRVP_LCD_WIGTH);   
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
	DCMI->CR|=(0x01<<5);//PLCK时钟极性
	DCMI->CR&=~(0x01<<3);//正常RGB格式，而非GPEG格式
	DCMI->CR&=~(0x01<<2);//捕获所有图像，不经过裁剪
	DCMI->CR&=~(0x01<<1);//连续捕获模式
	DCMI->IER|=(0x01<<0);//开启捕获完成中断
	
	/* 下面是DMA的配置，一部分配置单独给出API使用寄存器方式 */
	DMA_HandleTypeDef hdma;
	hdma.Instance=DMA1_Stream6;
	hdma.Init.Direction=DMA_PERIPH_TO_MEMORY;
	hdma.Init.FIFOMode=DMA_FIFOMODE_ENABLE;
	hdma.Init.FIFOThreshold=DMA_FIFO_THRESHOLD_FULL;
	hdma.Init.MemBurst=DMA_MBURST_SINGLE;
	hdma.Init.MemDataAlignment=DMA_MDATAALIGN_WORD;//FMC模拟8080支持16位，如果不带显存的话是16，带的话是32
	hdma.Init.MemInc=DMA_MINC_ENABLE;//如果不带显存，使用FMC的话必须失能，否则使能
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
	DMA1_Stream6->CR|=(0x01<<4);//开启传输完成中断
	DMA1_Stream6->NDTR=CAM_DMA_WORDS;//配合循环模式刷全图
	DMA1_Stream6->PAR=(uint32_t)&DCMI->DR;//外设地址
	DMA1_Stream6->M0AR=(uint32_t)ov2640_frame_buf;//内存地址,FMC的地址
	HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0x08, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn); 
	
	DCMI->CR|=(0x01<<0);//使能DCMI捕获
	DMA1_Stream6->CR|=(0x01<<0);//开启DMA
	DCMI->CR|=(0x01<<14);//使能DCMI
	
	
}

void ov2640_display_frame_prc(void)
{
	if(ov2640_frame_tranflg)
	{
    uint16_t *pixels=(uint16_t *)ov2640_frame_buf;
	  uint32_t loopcnt=CAM_WIDTH*CAM_HEIGHT;

    drv_fmc_write_cmd(0x2A);
    drv_fmc_write_data(0);
    drv_fmc_write_data(0);
    drv_fmc_write_data((CAM_WIDTH-1U)>>8);
    drv_fmc_write_data((CAM_WIDTH-1U)&0xFF);

    drv_fmc_write_cmd(0x2B);
    drv_fmc_write_data(0);
    drv_fmc_write_data(0);
    drv_fmc_write_data((CAM_HEIGHT-1U)>>8);
    drv_fmc_write_data((CAM_HEIGHT-1U)&0xFF);

    drv_fmc_write_cmd(0x2C);
    for(uint32_t i=0;i<loopcnt;i++) drv_fmc_write_data(pixels[i]);
		
		ov2640_frame_tranflg=0;
	}
}

void DMA1_Stream6_IRQHandler(void)
{
	if(DMA1->HISR&(0x01<<21))
	{
		DMA1->HIFCR|=(0x01<<21);
		ov2640_frame_tranflg=1;
	}
}

void DCMI_IRQHandler(void)
{
	if(DCMI->SR&(0x01<<0))
	{
		DCMI->ICR|=(0x01<<0);
	}
}
