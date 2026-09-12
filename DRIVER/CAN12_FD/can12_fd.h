#ifndef _CAN12_FD_H_
#define _CAN12_FD_H_

#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdbool.h>

/* 根据宏定义来配置是使用CANFD格式的数据报还是标准CAN2.0格式的数据报 */
#define USE_CAN_FD_FOMAT     0
#define USE_STD_CAN_FOMAT    1

/* 不可同时使用 */
#if USE_CAN_FD_FOMAT && USE_STD_CAN_FOMAT
#error "USE_CAN_FD_FOMAT USE_STD_CAN_FOMAT ERROR"
#endif

typedef struct
{
	FDCAN_RxHeaderTypeDef RxHeader;
	uint8_t               pdata[64];//兼容8-64字节
}RX_FIFO_TYPE;

#define FDCAN_FIFO_SIZE  128

typedef struct
{
	volatile uint8_t write;
	volatile uint8_t read;
	RX_FIFO_TYPE  fifo[FDCAN_FIFO_SIZE];
}FDCAN_FIFO_TYPE;

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
//#define FDCAN_DLC_BYTES_12 ((uint32_t)0x00000009U) /*!< 12 bytes data field */
//#define FDCAN_DLC_BYTES_16 ((uint32_t)0x0000000AU) /*!< 16 bytes data field */
//#define FDCAN_DLC_BYTES_20 ((uint32_t)0x0000000BU) /*!< 20 bytes data field */
//#define FDCAN_DLC_BYTES_24 ((uint32_t)0x0000000CU) /*!< 24 bytes data field */
//#define FDCAN_DLC_BYTES_32 ((uint32_t)0x0000000DU) /*!< 32 bytes data field */
//#define FDCAN_DLC_BYTES_48 ((uint32_t)0x0000000EU) /*!< 48 bytes data field */
//#define FDCAN_DLC_BYTES_64 ((uint32_t)0x0000000FU) /*!< 64 bytes data field */
void can12_fd_init(void);
bool can12_fd_get_msg(RX_FIFO_TYPE* rmsg);
void can12_fd_send_msg_std(uint16_t id,uint32_t dlc,uint8_t* pdata,uint8_t msgid,bool is_data_frame);
void can12_fd_send_msg_ext(uint32_t id,uint32_t dlc,uint8_t* pdata,uint8_t msgid,bool is_data_frame);


#endif
