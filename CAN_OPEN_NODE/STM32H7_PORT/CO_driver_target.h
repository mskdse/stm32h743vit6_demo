#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ´¿AIÊÊÅä£¬½»¸øAI¼´¿É */
#include "bx_can12_open.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) (x)
#define CO_SWAP_32(x) (x)
#define CO_SWAP_64(x) (x)

typedef uint_fast8_t bool_t;
typedef float        float32_t;
typedef double       float64_t;

static inline uint8_t CO_CANrxMsg_getDLC(uint32_t dlc)
{
    switch(dlc)
    {
        case FDCAN_DLC_BYTES_0: return 0;
        case FDCAN_DLC_BYTES_1: return 1;
        case FDCAN_DLC_BYTES_2: return 2;
        case FDCAN_DLC_BYTES_3: return 3;
        case FDCAN_DLC_BYTES_4: return 4;
        case FDCAN_DLC_BYTES_5: return 5;
        case FDCAN_DLC_BYTES_6: return 6;
        case FDCAN_DLC_BYTES_7: return 7;
        case FDCAN_DLC_BYTES_8: return 8;
        default: return 0;
    }
}

static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg)
{
    RX_FIFO_TYPE *msg = (RX_FIFO_TYPE *)rxMsg;
    return (uint16_t)(msg->RxHeader.Identifier & 0x7FFU);
}

static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg)
{
    RX_FIFO_TYPE *msg = (RX_FIFO_TYPE *)rxMsg;
    return CO_CANrxMsg_getDLC(msg->RxHeader.DataLength);
}

static inline const uint8_t *CO_CANrxMsg_readData(void *rxMsg)
{
    RX_FIFO_TYPE *msg = (RX_FIFO_TYPE *)rxMsg;
    return msg->pdata;
}

typedef struct
{
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*CANrx_callback)(void *object, void *message);
} CO_CANrx_t;

typedef struct
{
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

typedef struct
{
    void *CANptr;
    CO_CANrx_t *rxArray;
    uint16_t rxSize;
    CO_CANtx_t *txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;
} CO_CANmodule_t;

typedef struct
{
    void *addr;
    size_t len;
    uint8_t subIndexOD;
    uint8_t attr;
    void *storageModule;
    uint16_t crc;
    size_t eepromAddrSignature;
    size_t eepromAddr;
    size_t offset;
    void *additionalParameters;
} CO_storage_entry_t;

#define CO_LOCK_CAN_SEND(CAN_MODULE)   __disable_irq()
#define CO_UNLOCK_CAN_SEND(CAN_MODULE) __enable_irq()

#define CO_LOCK_EMCY(CAN_MODULE)       __disable_irq()
#define CO_UNLOCK_EMCY(CAN_MODULE)     __enable_irq()

#define CO_LOCK_OD(CAN_MODULE)         __disable_irq()
#define CO_UNLOCK_OD(CAN_MODULE)       __enable_irq()

#define CO_MemoryBarrier() __DMB()

#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)

#define CO_FLAG_SET(rxNew) \
    do { \
        CO_MemoryBarrier(); \
        (rxNew) = (void *)1L; \
    } while(0)

#define CO_FLAG_CLEAR(rxNew) \
    do { \
        CO_MemoryBarrier(); \
        (rxNew) = NULL; \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif
