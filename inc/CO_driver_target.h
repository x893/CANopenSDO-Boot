#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "at32f415.h"

typedef float float32_t;
typedef double float64_t;
typedef bool bool_t;

static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg)
{
    return ((can_rx_message_type*)rxMsg)->dlc;
}

static inline const uint8_t * CO_CANrxMsg_readData(void *rxMsg)
{
    return (const uint8_t*)((can_rx_message_type*)rxMsg)->data;
}

static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg)
{
    return ((can_rx_message_type*)rxMsg)->standard_id;
}

#define CO_LOCK_CAN_SEND(can)      __disable_irq()
#define CO_UNLOCK_CAN_SEND(can)    __enable_irq()
#define CO_LOCK_EMCY(can)           __disable_irq()
#define CO_UNLOCK_EMCY(can)         __enable_irq()
#define CO_LOCK_OD(can)             __disable_irq()
#define CO_UNLOCK_OD(can)           __enable_irq()

#define CO_FLAG_READ(rxNew)         (*(rxNew) != 0)
#define CO_FLAG_SET(rxNew)          (*(rxNew) = 1)
#define CO_FLAG_CLEAR(rxNew)        (*(rxNew) = 0)

typedef struct CO_CANrx_TAG {
    uint16_t ident;
    uint16_t mask;
    void *object;
    void (*pCANrx_callback)(void *object, void *message);
} CO_CANrx_t;

typedef struct {
    volatile bool_t bufferFull;
    uint8_t data[8];
    uint8_t DLC;
    bool_t rtr;
    bool_t syncFlag;
} CO_CANtx_t;
123
typedef struct {
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

#define CO_CONFIG_NMT                      CO_CONFIG_NMT_ENABLE
#define CO_CONFIG_HB_PRODUCER               CO_CONFIG_HB_PRODUCER_ENABLE
#define CO_CONFIG_EM                         CO_CONFIG_EM_ENABLE
#define CO_CONFIG_SDO_SRV                    (CO_CONFIG_SDO_SRV_ENABLE | \
                                               CO_CONFIG_SDO_SRV_BLOCK_TRANSFER)
#define CO_CONFIG_SDO_SRV_BUFFER_SIZE        2048
#define CO_CONFIG_LSS                         (CO_CONFIG_LSS_SLAVE | \
                                                CO_CONFIG_LSS_FASTSCAN)
#define CO_CONFIG_PDO                         (CO_CONFIG_RPDO_ENABLE | \
                                                CO_CONFIG_TPDO_ENABLE)
#define CO_CONFIG_RPDO_N                      2
#define CO_CONFIG_TPDO_N                      2
#define CO_CONFIG_STORAGE                      CO_CONFIG_STORAGE_ENABLE

// Отключаем ненужные функции
#define CO_CONFIG_SDO_CLI                     0
#define CO_CONFIG_SYNC                         0
#define CO_CONFIG_TIME                         0
#define CO_CONFIG_GATEWAY_ASCII                0
#define CO_CONFIG_SRDO                         0
#define CO_CONFIG_TRACE                        0

#endif /* CO_DRIVER_TARGET_H */
