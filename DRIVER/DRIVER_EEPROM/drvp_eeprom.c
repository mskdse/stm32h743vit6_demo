#include "drvp_eeprom.h"

static drvp_eeprom_type drvp_eeprom;

typedef struct
{
    void(*init)(void);

    void(*wbuf)(uint16_t DevAddress,
                uint16_t MemAddress,
                uint8_t* wbuf,
                uint16_t wsize);
    bool(*retwflg)(void);
    void(*clrwflg)(void);

    void(*rbuf)(uint16_t DevAddress,
                uint16_t MemAddress,
                uint8_t* rbuf,
                uint16_t rsize);
    bool(*retrflg)(void);
    void(*clrrflg)(void);

    bool(*reterrflg)(void);
    void(*clrerrflg)(void);
}drv_eeprom_type;

static const drv_eeprom_type drv_eeprom=
{
    .init = drv_eeprom_init,
    .wbuf = drv_eeprom_writebuf,
    .retwflg = drv_eeprom_retwflag,
    .clrwflg = drv_eeprom_clrwflag,
    .rbuf = drv_eeprom_readbuf,
    .retrflg = drv_eeprom_retrflag,
    .clrrflg = drv_eeprom_clrrflag,
    .reterrflg = drv_eeprom_reterrflag,
    .clrerrflg = drv_eeprom_clrerrflag,
};

void drvp_eeprom_init(void)
{
    drv_eeprom.init();

    drv_eeprom.rbuf(EEPROM_DEV_ADDR,0,drvp_eeprom.EEPROM_BUF,EEPROM_MAX_SIZE);

    /* 等待读取完成，超时计数防止死循环 */
    uint32_t timeout = 100000;
    while(!drv_eeprom.retrflg())
    {
        if(drv_eeprom.reterrflg())
        {
            drv_eeprom.clrerrflg();
            drv_eeprom.clrrflg();
            break;
        }
        if(--timeout == 0) break;
    }
    drv_eeprom.clrrflg();

    drvp_eeprom.state = EEPROM_STATE_IDLE;
    drvp_eeprom.busy = false;
}

bool drvp_eeprom_writebytes(uint16_t addr,
                            uint8_t *wbuf,
                            uint16_t wsize)
{
    if(((addr+wsize)>EEPROM_MAX_SIZE)||drvp_eeprom.busy) return false;

    for(uint16_t i=0;i<wsize;i++) drvp_eeprom.EEPROM_BUF[addr+i]=wbuf[i];

    drvp_eeprom.addr=addr;
    drvp_eeprom.remain=wsize;
    drvp_eeprom.busy=true;
    drvp_eeprom.state=EEPROM_STATE_WRITE_START;
    return true;

}

void drvp_eeprom_prc_10ms(void)
{
    switch (drvp_eeprom.state)
		{
       case EEPROM_STATE_WRITE_START:
			 {
            uint16_t offset = drvp_eeprom.addr % EEPROM_PAGE_SIZE;
            uint16_t page_remain = EEPROM_PAGE_SIZE - offset;

            if (drvp_eeprom.remain < page_remain)  drvp_eeprom.page_size = drvp_eeprom.remain;
            else                                   drvp_eeprom.page_size = page_remain;

            drvp_eeprom.page_addr = drvp_eeprom.addr;

            drv_eeprom.wbuf(
                EEPROM_DEV_ADDR,
                drvp_eeprom.page_addr,
                &drvp_eeprom.EEPROM_BUF[drvp_eeprom.page_addr],
                drvp_eeprom.page_size
            );

            drvp_eeprom.state = EEPROM_STATE_WRITE_WAIT;
        }
        break;

        case EEPROM_STATE_WRITE_WAIT:
			 {
            /* 先检查是否有 I2C 错误 */
            if(drv_eeprom.reterrflg())
            {
                drv_eeprom.clrerrflg();
                drv_eeprom.clrwflg();
                drvp_eeprom.busy = false;
                drvp_eeprom.state = EEPROM_STATE_IDLE;
                break;
            }

            if(drv_eeprom.retwflg())
			     {
                drv_eeprom.clrwflg();
                drvp_eeprom.addr += drvp_eeprom.page_size;
                drvp_eeprom.remain -= drvp_eeprom.page_size;
                if (drvp_eeprom.remain == 0)
					    	{
									drvp_eeprom.busy = false;
									drvp_eeprom.state = EEPROM_STATE_IDLE;
                } else drvp_eeprom.state = EEPROM_STATE_WRITE_START;
            }
        }
        break;

        case EEPROM_STATE_IDLE:
        default: break;
    }
}

void drvp_eeprom_readbytes(uint16_t addr,
                           uint8_t *rbuf,
                           uint16_t rsize)
{
    for(int i=0;i<rsize;i++) rbuf[i]=drvp_eeprom.EEPROM_BUF[addr+i];
}
