#ifndef _DRVP_EEPROM_H_
#define _DRVP_EEPROM_H_

#include "drv_eeprom.h"
#include <string.h>

#define EEPROM_A0           0
#define EEPROM_A1           0
#define EEPROM_A2           0

#define EEPROM_ADDR   ((0xA<<3)|(EEPROM_A2<<2)|(EEPROM_A1<<1)|(EEPROM_A0<<0))
#define EEPROM_DEV_ADDR (EEPROM_ADDR<<1)

#define EEPROM_MAX_SIZE     256
#define EEPROM_PAGE_SIZE    8

void drvp_eeprom_init(void);

void drvp_eeprom_writebytes(uint16_t addr,
                            uint8_t *wbuf,
                            uint16_t wsize);

void drvp_eeprom_readbytes(uint16_t addr,
                           uint8_t *rbuf,
                           uint16_t rsize);


#endif
