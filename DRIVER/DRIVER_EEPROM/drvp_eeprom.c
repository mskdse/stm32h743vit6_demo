#include "drvp_eeprom.h"

__attribute__((section (".RAM_D2")))static uint8_t EEPROM_BUF[EEPROM_MAX_SIZE];

typedef struct
{
    void(*init)(void);

    void(*wbuf)(uint16_t DevAddress,
                uint16_t MemAddress,
                uint8_t* wbuf,
                uint16_t wsize);
    void(*rbuf)(uint16_t DevAddress,
                uint16_t MemAddress,
                uint8_t* rbuf,
                uint16_t rsize);
}drv_eeprom_type;

static drv_eeprom_type drv_eeprom=
{
    .init = drv_eeprom_init,
    .wbuf = drv_eeprom_writebuf,
    .rbuf = drv_eeprom_readbuf,
};

void drvp_eeprom_init(void)
{
    drv_eeprom.init();
	  memset(EEPROM_BUF,0,EEPROM_MAX_SIZE);
	  drv_eeprom.rbuf(EEPROM_DEV_ADDR,0,EEPROM_BUF,EEPROM_MAX_SIZE);
}

void drvp_eeprom_writebytes(uint16_t addr,
                            uint8_t *wbuf,
                            uint16_t wsize)
{
	  uint16_t page_offset=0;
	  uint16_t page_remain=0;
	  uint16_t write_len=0;
	  
	  for(int i=0;i<wsize;i++) EEPROM_BUF[addr+i]=wbuf[i];
	  
    while (wsize > 0) 
		{
			/* 计算当前页剩余可写字节数 */
			page_offset = addr % EEPROM_PAGE_SIZE;
			page_remain = EEPROM_PAGE_SIZE - page_offset;

			/* 本次实际写入长度 = min(页剩余, 还需写入) */
			write_len = (wsize < page_remain) ? wsize : page_remain;

			/* 调用底层写接口 */
			drv_eeprom.wbuf(EEPROM_DEV_ADDR, addr, wbuf, write_len);

			/* 更新偏移 */
			addr += write_len;
			wbuf  += write_len;
			wsize -= write_len;
			
			for(int i=0;i<0xFFFFF;i++) __NOP();
    }
}

void drvp_eeprom_readbytes(uint16_t addr,
                           uint8_t *rbuf,
                           uint16_t rsize)
{
    for(int i=0;i<rsize;i++) rbuf[i]=EEPROM_BUF[addr+i];
}
