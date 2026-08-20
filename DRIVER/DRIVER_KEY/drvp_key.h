#ifndef _DRVP_KEY_H_
#define _DRVP_KEY_H_

#include "drv_key.h"
#include "circular_buffer.h"

typedef enum
{
	E_KEY_START,
	E_KEY_1=E_KEY_START,
	E_KEY_2,
	E_KEY_3,
	E_KEY_4,
	E_KEY_COUNT
}E_KEY_TYPE;

typedef enum
{
    E_EVENT_PRESS,//按下
    E_EVENT_RELEASE,//释放
    E_EVENT_LPRESS,//长按
    E_EVENT_LPRESS_RELEASE,//长按释放
    E_EVENT_CLICK,//单击
    E_EVENT_DOUBLE_CLICK,//双击
}E_EVENT_TYPE;

#define KEY_EVENT_ERROR    0xFFFF

void drvp_key_init(void);
uint16_t drvp_key_rfifo(void);
void drvp_key_prc_10ms(void);

#endif
