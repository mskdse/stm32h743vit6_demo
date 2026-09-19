#include "drv_eeprom.h"
#include <string.h>

__attribute__((section (".RAM_D2")))static I2C_HandleTypeDef  eepromi2c;
/* 涉及到HAL句柄(尤其是有回调函数)必须先memset清空为默认态，DeInit或者调用默认值赋值函数都不行。
   因为HAL的句柄有可能涉及到if判断，如果我的这个变量本身参数就没有初始为默认值
	 或者0，那么初始化函数里面判断不通过会少赋值，导致比如说回调函数未注册然后PC
	 指针跳转错误的一系列问题。
*/

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
  
	 memset(&eepromi2c,0,sizeof(I2C_HandleTypeDef));
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
}

void drv_eeprom_writebuf(uint16_t DevAddress,uint16_t MemAddress,uint8_t* wbuf,uint16_t wsize)
{
	HAL_I2C_Mem_Write(&eepromi2c,DevAddress,MemAddress,I2C_MEMADD_SIZE_8BIT,wbuf,wsize,HAL_MAX_DELAY);
}

void drv_eeprom_readbuf(uint16_t DevAddress,uint16_t MemAddress,uint8_t* rbuf,uint16_t rsize)
{
	HAL_I2C_Mem_Read(&eepromi2c,DevAddress,MemAddress,I2C_MEMADD_SIZE_8BIT,rbuf,rsize,HAL_MAX_DELAY);
}
