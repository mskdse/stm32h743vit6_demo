#ifndef _DRV_FMC_LCD_H_
#define _DRV_FMC_LCD_H_

#include "stm32h7xx_hal.h"
#include <stdbool.h>

void drv_fmc_lcd_init(void);
void drv_fmc_lcd_rst(void);
void drv_fmc_bl_ctrl(bool isopen);
void drv_fmc_write_cmd(uint16_t cmd);
void drv_fmc_write_data(uint16_t data);
uint16_t drv_fmc_read_data(void);


void drv_fmc_dma_m2m_init(void(*Callback)(DMA_HandleTypeDef *_hdma));
void drv_fmc_dma_m2m_tranf(uint16_t* buf,uint16_t len);

#endif
