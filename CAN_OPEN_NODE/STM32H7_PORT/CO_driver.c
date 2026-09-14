#include "301/CO_driver.h"
#include "bx_can12_open.h"

static uint16_t rxErrors = 0;
static uint16_t txErrors = 0;
static uint16_t overflow = 0;

static void CO_CANtx_process(CO_CANmodule_t *CANmodule)
{
    uint16_t i;
    CO_CANtx_t *buffer;
    uint16_t ident;
    bool_t rtr;

    if(CANmodule->CANtxCount == 0U)
        return;

    buffer = &CANmodule->txArray[0];

    for(i = 0U; i < CANmodule->txSize; i++, buffer++)
    {
        if(!buffer->bufferFull)
            continue;

        ident = (uint16_t)(buffer->ident & 0x07FFU);
        rtr = ((buffer->ident & 0x8000U) != 0U);

        if(rtr)
            continue;

        if(bx_can12_send_msg_std((FDCAN_GlobalTypeDef *)CANmodule->CANptr,
                                 ident,
                                 buffer->DLC,
                                 buffer->data,
                                 (uint8_t)i))
        {
            buffer->bufferFull = false;
            CANmodule->CANtxCount--;
            CANmodule->bufferInhibitFlag = buffer->syncFlag;
            CANmodule->firstCANtxMessage = false;
            break;
        }
    }

    if(CANmodule->CANtxCount == 0U)
        CANmodule->CANtxCount = 0U;
}

static void CO_CANrx_process(CO_CANmodule_t *CANmodule)
{
    RX_FIFO_TYPE rcvMsg;
    uint16_t index;
    uint16_t rcvMsgIdent;
    CO_CANrx_t *buffer;
    bool_t msgMatched;

    while (bx_can12_get_msg((FDCAN_GlobalTypeDef *)CANmodule->CANptr, &rcvMsg))
    {
        rcvMsgIdent = CO_CANrxMsg_readIdent(&rcvMsg);

        buffer = &CANmodule->rxArray[0];
        msgMatched = false;

        for (index = CANmodule->rxSize; index > 0U; index--)
        {
            if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U)
            {
                msgMatched = true;
                break;
            }
            buffer++;
        }

        if (msgMatched && buffer->CANrx_callback != NULL)
        {
            buffer->CANrx_callback(buffer->object, &rcvMsg);
        }
    }
}

void CO_CANsetConfigurationMode(void *CANptr)
{
    (void)CANptr;
}

void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    if (CANmodule != NULL)
    {
        CANmodule->CANnormal = true;
    }
}

CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule,
                                    void *CANptr,
                                    CO_CANrx_t rxArray[],
                                    uint16_t rxSize,
                                    CO_CANtx_t txArray[],
                                    uint16_t txSize,
                                    uint16_t CANbitRate)
{
    uint16_t i;

    (void)CANbitRate;

    if (CANmodule == NULL || rxArray == NULL || txArray == NULL)
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0U;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    for (i = 0U; i < rxSize; i++)
    {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }

    for (i = 0U; i < txSize; i++)
    {
        txArray[i].bufferFull = false;
        txArray[i].syncFlag = false;
    }

    rxErrors = 0U;
    txErrors = 0U;
    overflow = 0U;

    return CO_ERROR_NO;
}

void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    if (CANmodule != NULL)
    {
        CANmodule->CANnormal = false;
    }
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule,
                                     uint16_t index,
                                     uint16_t ident,
                                     uint16_t mask,
                                     bool_t rtr,
                                     void *object,
                                     void (*CANrx_callback)(void *object, void *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if ((CANmodule != NULL) &&
        (object != NULL) &&
        (CANrx_callback != NULL) &&
        (index < CANmodule->rxSize))
    {
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;
        buffer->ident = ident & 0x07FFU;

        if (rtr)
        {
            buffer->ident |= 0x0800U;
        }

        buffer->mask = (mask & 0x07FFU) | 0x0800U;
    }
    else
    {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule,
                               uint16_t index,
                               uint16_t ident,
                               bool_t rtr,
                               uint8_t noOfBytes,
                               bool_t syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if ((CANmodule != NULL) &&
        (index < CANmodule->txSize) &&
        (noOfBytes <= 8U))
    {
        buffer = &CANmodule->txArray[index];

        buffer->ident = ((uint32_t)ident & 0x07FFU) |
                        ((uint32_t)noOfBytes << 11U) |
                        ((uint32_t)(rtr ? 0x8000U : 0U));

        buffer->DLC = noOfBytes;
        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;

    if (CANmodule == NULL || buffer == NULL)
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    if (buffer->bufferFull)
    {
        if (!CANmodule->firstCANtxMessage)
        {
            CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        }

        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND(CANmodule);

    if (CANmodule->CANtxCount == 0U)
    {
        uint16_t ident;
        bool_t rtr;

        ident = (uint16_t)(buffer->ident & 0x07FFU);
        rtr = ((buffer->ident & 0x8000U) != 0U);

        if (!rtr &&
            bx_can12_send_msg_std((FDCAN_GlobalTypeDef *)CANmodule->CANptr,
                                  ident,
                                  buffer->DLC,
                                  buffer->data,
                                  0U))
        {
            CANmodule->bufferInhibitFlag = buffer->syncFlag;
            CANmodule->firstCANtxMessage = false;
        }
        else
        {
            if (!buffer->bufferFull)
            {
                buffer->bufferFull = true;
                CANmodule->CANtxCount++;
            }
        }
    }
    else
    {
        if (!buffer->bufferFull)
        {
            buffer->bufferFull = true;
            CANmodule->CANtxCount++;
        }
    }

    CO_UNLOCK_CAN_SEND(CANmodule);

    return err;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    uint32_t tpdoDeleted = 0U;
    uint16_t i;

    if (CANmodule == NULL)
    {
        return;
    }

    CO_LOCK_CAN_SEND(CANmodule);

    if (CANmodule->bufferInhibitFlag)
    {
        CANmodule->bufferInhibitFlag = false;
        tpdoDeleted = 1U;
    }

    if (CANmodule->CANtxCount != 0U)
    {
        CO_CANtx_t *buffer = &CANmodule->txArray[0];

        for (i = CANmodule->txSize; i > 0U; i--)
        {
            if (buffer->bufferFull && buffer->syncFlag)
            {
                buffer->bufferFull = false;

                if (CANmodule->CANtxCount > 0U)
                {
                    CANmodule->CANtxCount--;
                }

                tpdoDeleted = 2U;
            }

            buffer++;
        }
    }

    CO_UNLOCK_CAN_SEND(CANmodule);

    if (tpdoDeleted != 0U)
    {
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}

void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    uint32_t err;

    if(CANmodule == NULL)
        return;

    CO_CANrx_process(CANmodule);
    CO_CANtx_process(CANmodule);

    err = ((uint32_t)txErrors << 16) |
          ((uint32_t)rxErrors << 8) |
          overflow;

    if(CANmodule->errOld != err)
    {
        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if(txErrors >= 256U)
        {
            status |= CO_CAN_ERRTX_BUS_OFF;
        }
        else
        {
            status &= 0xFFFFU ^
                       (CO_CAN_ERRTX_BUS_OFF |
                        CO_CAN_ERRRX_WARNING |
                        CO_CAN_ERRRX_PASSIVE |
                        CO_CAN_ERRTX_WARNING |
                        CO_CAN_ERRTX_PASSIVE);

            if(rxErrors >= 128U)
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE;
            else if(rxErrors >= 96U)
                status |= CO_CAN_ERRRX_WARNING;

            if(txErrors >= 128U)
                status |= CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE;
            else if(txErrors >= 96U)
                status |= CO_CAN_ERRTX_WARNING;

            if((status & CO_CAN_ERRTX_PASSIVE) == 0U)
                status &= 0xFFFFU ^ CO_CAN_ERRTX_OVERFLOW;
        }

        if(overflow != 0U)
            status |= CO_CAN_ERRRX_OVERFLOW;

        CANmodule->CANerrorStatus = status;
    }
}

void CO_CANinterrupt(CO_CANmodule_t *CANmodule)
{
    CO_CANrx_process(CANmodule);
}
