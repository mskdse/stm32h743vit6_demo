#include "drv_eeprom.h"

static I2C_HandleTypeDef  eepromi2c;
volatile bool      eepromwendflag=false;
volatile bool      eepromrendflag=false;
volatile bool      eepromerrflag=false;

void drv_eeprom_init(void)
{
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_I2C2_CLK_ENABLE();

	GPIO_InitTypeDef gpio_cfg;
	gpio_cfg.Alternate=GPIO_AF4_I2C2;
	gpio_cfg.Mode=GPIO_MODE_AF_OD;
	gpio_cfg.Pin=GPIO_PIN_10|GPIO_PIN_11;
	gpio_cfg.Pull=GPIO_PULLUP;
	gpio_cfg.Speed=GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB,&gpio_cfg);

	 eepromi2c.Instance=I2C2;
	 eepromi2c.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
	 eepromi2c.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE;
	 eepromi2c.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE;
	 eepromi2c.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
	 eepromi2c.Init.OwnAddress1=0x00;
	 eepromi2c.Init.OwnAddress2=0x00;
	 eepromi2c.Init.OwnAddress2Masks=I2C_OA2_NOMASK;
	 eepromi2c.Init.Timing=0x10420F13;
	 HAL_I2C_Init(&eepromi2c);

	HAL_NVIC_SetPriority(I2C2_EV_IRQn,13,0);
	HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);

	HAL_NVIC_SetPriority(I2C2_ER_IRQn,13,0);
	HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
}

void drv_eeprom_writebuf(uint16_t DevAddress,uint16_t MemAddress,uint8_t* wbuf,uint16_t wsize)
{
	eepromerrflag=false;
	HAL_I2C_Mem_Write_IT(&eepromi2c,DevAddress,MemAddress,I2C_MEMADD_SIZE_8BIT,wbuf,wsize);
}

bool drv_eeprom_retwflag(void)
{
	return eepromwendflag;
}

void drv_eeprom_clrwflag(void)
{
	 eepromwendflag=false;
}

void drv_eeprom_readbuf(uint16_t DevAddress,uint16_t MemAddress,uint8_t* rbuf,uint16_t rsize)
{
	eepromerrflag=false;
	HAL_I2C_Mem_Read_IT(&eepromi2c,DevAddress,MemAddress,I2C_MEMADD_SIZE_8BIT,rbuf,rsize);
}

bool drv_eeprom_retrflag(void)
{
	return eepromrendflag;
}

void drv_eeprom_clrrflag(void)
{
	 eepromrendflag=false;
}

bool drv_eeprom_reterrflag(void)
{
	return eepromerrflag;
}

void drv_eeprom_clrerrflag(void)
{
	eepromerrflag=false;
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if(I2C2==hi2c->Instance) eepromwendflag=true;
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if(I2C2==hi2c->Instance) eepromrendflag=true;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	if(I2C2==hi2c->Instance)
	{
		 HAL_I2C_DeInit(&eepromi2c);
		 eepromi2c.Instance=I2C2;
		 eepromi2c.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
		 eepromi2c.Init.DualAddressMode=I2C_DUALADDRESS_DISABLE;
		 eepromi2c.Init.GeneralCallMode=I2C_GENERALCALL_DISABLE;
		 eepromi2c.Init.NoStretchMode=I2C_NOSTRETCH_DISABLE;
		 eepromi2c.Init.OwnAddress1=0x00;
		 eepromi2c.Init.OwnAddress2=0x00;
		 eepromi2c.Init.OwnAddress2Masks=I2C_OA2_NOMASK;
		 eepromi2c.Init.Timing=0x10420F13;
		 HAL_I2C_Init(&eepromi2c);
		 eepromerrflag=true;
	}
}

void I2C2_EV_IRQHandler(void)
{
	HAL_I2C_EV_IRQHandler(&eepromi2c);
}

void I2C2_ER_IRQHandler(void)
{
	HAL_I2C_ER_IRQHandler(&eepromi2c);
}
