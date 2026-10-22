#ifndef _OV2640_H_
#define _OV2640_H_

#include "stm32h7xx_hal.h"

#define OV2640_DEVADDR_7BIT    0x30
#define OV2640_DEVADDR_8BIT   ((OV2640_DEVADDR_7BIT<<1)|0)

void ov2640_init(void);
uint16_t ov2640_read_pid(void);
void ov2640_uxga_config(void);
void ov2640_dma_transfer_start(void);
void ov2640_display_frame_prc(void);

#endif
