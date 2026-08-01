#ifndef _DRVP_EEPROM_H_
#define _DRVP_EEPROM_H_

#include "drv_eeprom.h"
#include <stdbool.h>
#include <stdint.h>

#define EEPROM_A0           0
#define EEPROM_A1           0
#define EEPROM_A2           0

#define EEPROM_ADDR   ((0xA<<3)|(EEPROM_A2<<2)|(EEPROM_A1<<1)|(EEPROM_A0<<0))
#define EEPROM_DEV_ADDR (EEPROM_ADDR<<1)

#define EEPROM_MAX_SIZE     256
#define EEPROM_PAGE_SIZE    8

typedef enum
{
    EEPROM_STATE_IDLE = 0,       //空闲
    EEPROM_STATE_WRITE_START,    //开始一个page写
    EEPROM_STATE_WRITE_WAIT      //等待EEPROM内部写完成
}EEPROM_STATE;

typedef struct
{
    uint8_t EEPROM_BUF[EEPROM_MAX_SIZE];

    /* 当前写进度 */
    uint16_t addr;        //当前写地址
    uint16_t remain;      //剩余写数量

    /* 当前page */
    uint16_t page_addr;
    uint16_t page_size;

    EEPROM_STATE state;

    bool busy;
}drvp_eeprom_type;

void drvp_eeprom_init(void);

bool drvp_eeprom_writebytes(uint16_t addr,
                            uint8_t *wbuf,
                            uint16_t wsize);//如果返回false就代表忙或者超出范围

void drvp_eeprom_readbytes(uint16_t addr,
                           uint8_t *rbuf,
                           uint16_t rsize);

void drvp_eeprom_prc_10ms(void);


#endif
