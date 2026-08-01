#ifndef _DRV_EEPROM_H_
#define _DRV_EEPROM_H_

#include "stm32h7xx_hal.h"
#include <stdbool.h>

void drv_eeprom_init(void);

void drv_eeprom_writebuf(uint16_t DevAddress,uint16_t MemAddress,uint8_t* wbuf,uint16_t wsize);
bool drv_eeprom_retwflag(void);
void drv_eeprom_clrwflag(void);

void drv_eeprom_readbuf(uint16_t DevAddress,uint16_t MemAddress,uint8_t* rbuf,uint16_t rsize);
bool drv_eeprom_retrflag(void);
void drv_eeprom_clrrflag(void);

bool drv_eeprom_reterrflag(void);
void drv_eeprom_clrerrflag(void);

#endif
