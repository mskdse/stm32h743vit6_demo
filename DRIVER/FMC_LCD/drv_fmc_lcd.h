#ifndef _DRV_FMC_LCD_H_
#define _DRV_FMC_LCD_H_

#include "stm32h7xx_hal.h"
#include <stdbool.h>

/* CS引脚接到FMC的BANK1的第一片区域，而且D/C复用为A19。
   访问命令的话地址肯定为0x60000000
   访问数据的话地址肯定为0x60080000(第19位置位),但是由于16位宽访问下
	 它会访问右移一位的地址，所以我们必须手动向左移一位是0x60100000(第20位置位)，访问命令的话不用管，因为此时D/C无论如何都是0
*/
#define FMC_LCD_ADDR_CMD     ((uint32_t)0x60000000)
#define FMC_LCD_ADDR_DATA    ((uint32_t)0x60100000)

void drv_fmc_lcd_init(void);
void drv_fmc_lcd_rst(void);
void drv_fmc_bl_ctrl(bool isopen);
void drv_fmc_write_cmd(uint16_t cmd);
void drv_fmc_write_data(uint16_t data);
uint16_t drv_fmc_read_data(void);


void drv_fmc_dma_m2m_init(void(*Callback)(DMA_HandleTypeDef *_hdma));
void drv_fmc_dma_m2m_tranf(uint16_t* buf,uint16_t len);

#endif
