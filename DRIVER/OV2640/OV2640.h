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

/* 这两个函数一个是跟野火上位机建立通信，一个是传输摄像头数据 */
void ov2640_frame_fire_init(void);
void ov2640_frame_fire_prc(void);

#endif
