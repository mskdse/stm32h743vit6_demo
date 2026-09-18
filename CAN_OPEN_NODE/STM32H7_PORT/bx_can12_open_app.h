#ifndef _BX_CAN12_OPEN_APP_H_
#define _BX_CAN12_OPEN_APP_H_

#include "CANopen.h"
#include "OD.h"
#include "bx_can12_open.h"
#include "usart1_dma.h"

void bx_can12_open_app_init(void);
void bx_can12_open_app_prc(void);
void bx_can12_open_app_prc_1ms(void);

#endif
