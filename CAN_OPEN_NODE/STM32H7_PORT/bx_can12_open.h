#ifndef _BX_CAN12_OPEN_H_
#define _BX_CAN12_OPEN_H_

/* 摘抄我的Driver驱动文件夹下的can12_fd.c/h的底层驱动，让他适配can open协议栈，额外的代码统统精简 */
#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdbool.h>

typedef struct
{
	FDCAN_RxHeaderTypeDef RxHeader;
	uint8_t               pdata[8];//8字节
}RX_FIFO_TYPE;

#define FDCAN_FIFO_SIZE  256

typedef struct
{
	volatile uint16_t write;
	volatile uint16_t read;
	RX_FIFO_TYPE  fifo[FDCAN_FIFO_SIZE];
}FDCAN_FIFO_TYPE;

/* CANIndex这个参数只需要给定FDCAN1/2即可 */
//FDCAN1
//FDCAN2

/* dlc这个参数只能由下面这些宏定义来给注意如果是CAN2.0标准的报文只能给0-8之间的参数 */
//#define FDCAN_DLC_BYTES_0  ((uint32_t)0x00000000U) /*!< 0 bytes data field  */
//#define FDCAN_DLC_BYTES_1  ((uint32_t)0x00000001U) /*!< 1 bytes data field  */
//#define FDCAN_DLC_BYTES_2  ((uint32_t)0x00000002U) /*!< 2 bytes data field  */
//#define FDCAN_DLC_BYTES_3  ((uint32_t)0x00000003U) /*!< 3 bytes data field  */
//#define FDCAN_DLC_BYTES_4  ((uint32_t)0x00000004U) /*!< 4 bytes data field  */
//#define FDCAN_DLC_BYTES_5  ((uint32_t)0x00000005U) /*!< 5 bytes data field  */
//#define FDCAN_DLC_BYTES_6  ((uint32_t)0x00000006U) /*!< 6 bytes data field  */
//#define FDCAN_DLC_BYTES_7  ((uint32_t)0x00000007U) /*!< 7 bytes data field  */
//#define FDCAN_DLC_BYTES_8  ((uint32_t)0x00000008U) /*!< 8 bytes data field  */
void bx_can12_init(bool use_canfd1,bool use_canfd2);
bool bx_can12_send_msg_std(FDCAN_GlobalTypeDef *CANIndex,uint16_t id,uint32_t dlc,uint8_t* pdata,uint8_t msgid);
bool bx_can12_get_msg(FDCAN_GlobalTypeDef *CANIndex,RX_FIFO_TYPE* rmsg);

#endif
