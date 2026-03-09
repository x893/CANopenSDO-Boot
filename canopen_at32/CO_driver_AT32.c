#include "boards.h"
#include "CO_driver_target.h"
#include "CO_app_AT32.h"
#include "301/CO_driver.h"

/* Local CAN module object */
static CO_CANmodule_t* CANModule_local = NULL; /* Local instance of global CAN module */

#define CAN_RX_QUEUE_SIZE   64 /* must be power of 2 */
#define CAN_TX_QUEUE_SIZE   64 /* must be power of 2 */

typedef struct { // CAN Rx queue
	uint16_t size;
	uint16_t head;
	CAN_RX_QUEUE_TYPE queue[CAN_RX_QUEUE_SIZE];
} CANRxQueue;

typedef struct { // CAN Tx queue
	uint16_t size;
	uint16_t head;
	CAN_TX_QUEUE_TYPE queue[CAN_TX_QUEUE_SIZE];
} CANTxQueue;

static CANRxQueue canRxQueue = {
  .size = 0,
  .head = 0
};

static CANTxQueue canTxQueue = {
  .size = 0,
  .head = 0
};

static inline bool_t CO_is_sdo_req_id(uint16_t canId)
{
	return (canId >= 0x600U) && (canId <= 0x67FU);
}

static inline bool_t CO_is_sdo_resp_id(uint16_t canId)
{
	return (canId >= 0x580U) && (canId <= 0x5FFU);
}

bool_t CANTxQueuePut(CAN_TX_QUEUE_TYPE* packet)
{
	bool_t result = false;

	if (packet == NULL)
		return false;

	CO_LOCK();
	if (canTxQueue.size < CAN_TX_QUEUE_SIZE)
	{
		uint16_t putPos = (canTxQueue.head + canTxQueue.size) & (CAN_TX_QUEUE_SIZE - 1);
		memcpy((uint8_t*)&(canTxQueue.queue[putPos]), (uint8_t*)packet, sizeof(CAN_TX_QUEUE_TYPE));
		canTxQueue.size++;
		result = true;
	}
	CO_UNLOCK();

	return result;
}

static CAN_TX_QUEUE_TYPE* CANTxQueueGet(void)
{
	if (canTxQueue.size == 0)
		return NULL;
	return &(canTxQueue.queue[canTxQueue.head]);
}

static void CANTxQueueShift(void)
{
	if (canTxQueue.size != 0)
	{
		CO_LOCK();
		canTxQueue.head = (canTxQueue.head + 1) & (CAN_TX_QUEUE_SIZE - 1);
		canTxQueue.size--;
		CO_UNLOCK();
	}
}

/*
 *
 */
bool_t CANRxQueuePut(CAN_RX_QUEUE_TYPE* packet)
{
	bool_t result = false;

	if (packet == NULL)
		return false;

	CO_LOCK();
	if (canRxQueue.size < CAN_RX_QUEUE_SIZE)
	{
		uint16_t putPos = (canRxQueue.head + canRxQueue.size) & (CAN_RX_QUEUE_SIZE - 1);
		memcpy((uint8_t*)&(canRxQueue.queue[putPos]), (uint8_t*)packet, sizeof(CAN_RX_QUEUE_TYPE));
		canRxQueue.size++;
		result = true;
	}
	CO_UNLOCK();

	return result;
}

bool_t CANQueuePutMirror(CAN_RX_QUEUE_TYPE* rxPacket, CAN_TX_QUEUE_TYPE* txPacket)
{
	bool_t result = false;

	if (rxPacket == NULL || txPacket == NULL)
		return false;

	CO_LOCK();
	if ((canRxQueue.size < CAN_RX_QUEUE_SIZE) && (canTxQueue.size < CAN_TX_QUEUE_SIZE))
	{
		uint16_t rxPutPos = (canRxQueue.head + canRxQueue.size) & (CAN_RX_QUEUE_SIZE - 1);
		uint16_t txPutPos = (canTxQueue.head + canTxQueue.size) & (CAN_TX_QUEUE_SIZE - 1);
		memcpy((uint8_t*)&(canRxQueue.queue[rxPutPos]), (uint8_t*)rxPacket, sizeof(CAN_RX_QUEUE_TYPE));
		memcpy((uint8_t*)&(canTxQueue.queue[txPutPos]), (uint8_t*)txPacket, sizeof(CAN_TX_QUEUE_TYPE));
		canRxQueue.size++;
		canTxQueue.size++;
		result = true;
	}
	CO_UNLOCK();

	return result;
}

static CAN_RX_QUEUE_TYPE* CANRxQueueGet(void)
{
	if (canRxQueue.size == 0)
		return NULL;
	return &(canRxQueue.queue[canRxQueue.head]);
}

void CANRxQueueShift(void)
{
	if (canRxQueue.size != 0)
	{
		CO_LOCK();
		canRxQueue.head = (canRxQueue.head + 1) & (CAN_RX_QUEUE_SIZE - 1);
		canRxQueue.size--;
		CO_UNLOCK();
	}
}

bool_t CANSendDataToQueue(uint16_t canId, uint8_t dataLength, uint8_t* data)
{
	CAN_TX_QUEUE_TYPE tmpQueueItem;

	tmpQueueItem.id_type = CAN_ID_STANDARD;
	tmpQueueItem.standard_id = canId;
	tmpQueueItem.dlc = dataLength;
	tmpQueueItem.frame_type = CAN_TFT_DATA;
	if (dataLength != 0)
		memcpy((uint8_t*)&(tmpQueueItem.data[0]), (uint8_t*)data, dataLength);

	return CANTxQueuePut(&tmpQueueItem);
}

/* CAN masks for identifiers */
#define CANID_MASK 0x07FF /*!< CAN standard ID mask */
#define FLAG_RTR   0x8000 /*!< RTR flag, part of identifier */

/******************************************************************************/
void CO_CANsetConfigurationMode(void* CANptr)
{
	/* Put CAN module in configuration mode */
	if (CANptr != NULL && ((CANOpenNodeAT32_t *)CANptr)->CANHandle != NULL)
		can_reset(((CANOpenNodeAT32_t *)CANptr)->CANHandle);
}

/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t* CANmodule)
{
	if (CANmodule->CANptr != NULL && ((CANOpenNodeAT32_t *)CANmodule->CANptr)->CANHandle != NULL)
		can_reset(((CANOpenNodeAT32_t *)CANmodule->CANptr)->CANHandle);
}

/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t* CANmodule)
{
	/* Put CAN module in normal mode */
	if (CANmodule->CANptr != NULL)
		CANmodule->CANnormal = true;
}

/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
	CO_CANmodule_t* CANmodule,
	void* CANptr,
	CO_CANrx_t rxArray[],
	uint16_t rxSize,
	CO_CANtx_t txArray[],
	uint16_t txSize,
	uint16_t CANbitRate
)
{
	/* verify arguments */
	if (CANmodule == NULL || rxArray == NULL || txArray == NULL)
		return CO_ERROR_ILLEGAL_ARGUMENT;

	/* Hold CANModule variable */
	CANmodule->CANptr = CANptr;

	/* Keep a local copy of CANModule */
	CANModule_local = CANmodule;

	/* Configure object variables */
	CANmodule->rxArray = rxArray;
	CANmodule->rxSize = rxSize;
	CANmodule->txArray = txArray;
	CANmodule->txSize = txSize;
	CANmodule->CANerrorStatus = 0;
	CANmodule->CANnormal = false;
	CANmodule->bufferInhibitFlag = false;
	CANmodule->firstCANtxMessage = true;
	CANmodule->CANtxCount = 0U;
	CANmodule->errOld = 0U;

	/* Reset all variables */
	for (int i = 0U; i < rxSize; i++)
	{
		rxArray[i].ident = 0U;
		rxArray[i].mask = 0xFFFFU;
		rxArray[i].object = NULL;
		rxArray[i].CANrx_callback = NULL;
	}

	for (int i = 0U; i < txSize; i++)
		txArray[i].bufferFull = false;

	if (CANbitRate != 0)
		((CANOpenNodeAT32_t *)CANptr)->baudrate = CANbitRate;

	if (((CANOpenNodeAT32_t *)CANptr)->HWInitFunction(CANptr) != SUCCESS)
		return CO_ERROR_NO;
	return CO_ERROR_NO;
}

/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
	CO_CANmodule_t* CANmodule,
	uint16_t index,
	uint16_t ident,
	uint16_t mask,
	bool_t rtr,
	void* object,
	void (*CANrx_callback)(void* object, void* message)
)
{
	if (CANmodule == NULL || object == NULL || CANrx_callback == NULL || index >= CANmodule->rxSize)
		return CO_ERROR_ILLEGAL_ARGUMENT;

	CO_CANrx_t* buffer = &CANmodule->rxArray[index];

	/* Configure object variables */
	buffer->object = object;
	buffer->CANrx_callback = CANrx_callback;

	/*
	 * Configure global identifier, including RTR bit
	 *
	 * This is later used for RX operation match case
	 */
	buffer->ident = (ident & CANID_MASK) | (rtr ? FLAG_RTR : 0x00);
	buffer->mask = (mask & CANID_MASK) | FLAG_RTR;

	return CO_ERROR_NO;
}

/******************************************************************************/
CO_CANtx_t* CO_CANtxBufferInit(
	CO_CANmodule_t* CANmodule,
	uint16_t index,
	uint16_t ident,
	bool_t rtr,
	uint8_t noOfBytes,
	bool_t syncFlag
)
{
	CO_CANtx_t* buffer = NULL;

	if (CANmodule != NULL && index < CANmodule->txSize)
	{
		buffer = &CANmodule->txArray[index];

		/* CAN identifier, DLC and rtr, bit aligned with CAN module transmit buffer */
		buffer->ident = ((uint32_t)ident & CANID_MASK) | ((uint32_t)(rtr ? FLAG_RTR : 0x00));
		buffer->DLC = noOfBytes;
		buffer->bufferFull = false;
		buffer->syncFlag = syncFlag;
	}
	return buffer;
}

/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t* CANmodule, CO_CANtx_t* buffer)
{
	CO_ReturnError_t err = CO_ERROR_NO;

	/* Verify overflow */
	if (buffer->bufferFull)
	{
		/* don't set error, if bootup message is still on buffers */
		if (!CANmodule->firstCANtxMessage)
			CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
		err = CO_ERROR_TX_OVERFLOW;
	}

	if (!CANSendDataToQueue(buffer->ident & 0x7FF, buffer->DLC & 0xFF, &buffer->data[0]))
		err = CO_ERROR_TX_OVERFLOW;
	return err;
}

/******************************************************************************/
#if ((CO_CONFIG_SYNC)&CO_CONFIG_SYNC_ENABLE) != 0
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t* CANmodule)
{
	uint32_t tpdoDeleted = 0U;

	CO_LOCK_CAN_SEND(CANmodule);
	/* Abort message from CAN module, if there is synchronous TPDO.
	 * Take special care with this functionality. */
	if (/*messageIsOnCanBuffer && */CANmodule->bufferInhibitFlag)
	{
		/* clear TXREQ */
		CANmodule->bufferInhibitFlag = false;
		tpdoDeleted = 1U;
	}
	/* delete also pending synchronous TPDOs in TX buffers */
	if (CANmodule->CANtxCount > 0)
	{
		for (uint16_t i = CANmodule->txSize; i > 0U; --i)
		{
			uint16_t idx = i - 1U;
			if (CANmodule->txArray[idx].bufferFull)
			{
				if (CANmodule->txArray[idx].syncFlag)
				{
					CANmodule->txArray[idx].bufferFull = false;
					CANmodule->CANtxCount--;
					tpdoDeleted = 2U;
				}
			}
		}
	}
	CO_UNLOCK_CAN_SEND(CANmodule);
	if (tpdoDeleted)
	{
		CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
	}
}
#endif

/******************************************************************************/
/* Get error counters from the module. If necessary, function may use
 * different way to determine errors. */
 // static uint16_t rxErrors = 0, txErrors = 0, overflow = 0;

#define CAN_ERR_ESTS_EAF	((uint32_t)0x00000001)
#define CAN_ERR_ESTS_EPF	((uint32_t)0x00000002)
#define CAN_ERR_ESTS_BOF	((uint32_t)0x00000004)
#define CAN_ERR_ESTS_ETR	((uint32_t)0x00000070)
#define CAN_ERR_ETR_NOERR			0x00	/*!< no error */
#define CAN_ERR_ETR_STUFFERR		0x01	/*!< stuff error */
#define CAN_ERR_ETR_FORMERR			0x02	/*!< form error */
#define CAN_ERR_ETR_ACKERR			0x03	/*!< acknowledgment error */
#define CAN_ERR_ETR_BITRECESSIVEERR	0x04	/*!< bit recessive error */
#define CAN_ERR_ETR_BITDOMINANTERR	0x05	/*!< bit dominant error */
#define CAN_ERR_ETR_CRCERR			0x06	/*!< crc error */
#define CAN_ERR_ETR_SOFTWARESETERR	0x07	/*!< software set error */

#define CAN_ERR_ESTS_TEC	((uint32_t)0xFF000000)
#define CAN_ERR_ESTS_REC	((uint32_t)0x00FF0000)
#define CAN_ERR_ESTS_ALL	(uint32_t)(CAN_ERR_ESTS_EAF | CAN_ERR_ESTS_EPF | CAN_ERR_ESTS_BOF | CAN_ERR_ESTS_ETR)

void CO_CANmodule_process(CO_CANmodule_t* CANmodule)
{
	can_type* can = ((CANOpenNodeAT32_t *)CANmodule->CANptr)->CANHandle;
	uint32_t err = can->ests & CAN_ERR_ESTS_ALL;
	if (err != 0)
	{
		can_flag_clear(CAN_CAN, CAN_ETR_FLAG);

		if (CANmodule->errOld != err)
		{
			uint16_t status = CANmodule->CANerrorStatus;
			CANmodule->errOld = err;
			if (err & CAN_ERR_ESTS_BOF)
			{
				// In this driver, we assume that auto bus recovery is activated !
				// so this error will eventually handled automatically.
				status |= CO_CAN_ERRTX_BUS_OFF;
			}
			else
			{
				/* recalculate CANerrorStatus, first clear some flags */
				status &= ~(
					CO_CAN_ERRTX_BUS_OFF |
					CO_CAN_ERRRX_WARNING |
					CO_CAN_ERRRX_PASSIVE |
					CO_CAN_ERRTX_WARNING |
					CO_CAN_ERRTX_PASSIVE
					);

				if (err & CAN_ERR_ESTS_EAF)
					status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRTX_WARNING;
				if (err & CAN_ERR_ESTS_EPF)
					status |= CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_PASSIVE;
			}
			CANmodule->CANerrorStatus = status;
		}
	}
}


#ifdef CAN_TX_IRQn
/**
 * \brief           TX buffer has been well transmitted callback
 * \param[in]       hcan: pointer to an CAN_HandleTypeDef structure that contains
 *                      the configuration information for the specified CAN.
 * \param[in]       MailboxNumber: the mailbox number that has been transmitted
 */
void CO_CANinterrupt_TX(void)
{
	CANModule_local->firstCANtxMessage = false; /* First CAN message (bootup) was sent successfully */
	CANModule_local->bufferInhibitFlag = false; /* Clear flag from previous message */
	if (CANModule_local->CANtxCount > 0U)
	{ /* Are there any new messages waiting to be send */
		CO_CANtx_t* buffer = &CANModule_local->txArray[0]; /* Start with first buffer handle */
		uint16_t i;

		/*
		 * Try to send more buffers, process all empty ones
		 *
		 * This function is always called from interrupt,
		 * however to make sure no preemption can happen, interrupts are anyway locked
		 * (unless you can guarantee no higher priority interrupt will try to access to CAN instance and send data,
		 *  then no need to lock interrupts..)
		 */
		CO_LOCK();
		for (i = CANModule_local->txSize; i > 0U; --i, ++buffer)
		{
			/* Try to send message */
			if (buffer->bufferFull)
			{
				if (prv_send_can_message(CANModule_local, buffer) == ERR_NONE)
				{
					buffer->bufferFull = false;
					CANModule_local->CANtxCount--;
					CANModule_local->bufferInhibitFlag = buffer->syncFlag;
				}
			}
		}
		/* Clear counter if no more messages */
		if (i == 0U)
			CANModule_local->CANtxCount = 0U;

		CO_UNLOCK();
	}
}
#endif

void CO_CANinterrupt_RX(CAN_RX_QUEUE_TYPE* rxMsg)
{
	CO_CANrx_t* msgBuff = CANModule_local->rxArray;
	uint16_t rxSize = CANModule_local->rxSize;
	uint16_t msg = rxMsg->standard_id;

	for (int index = 0; index < rxSize; index++)
	{
		if (((msg ^ msgBuff->ident) & msgBuff->mask) == 0)
		{
			if (msgBuff->CANrx_callback)
				msgBuff->CANrx_callback(msgBuff->object, rxMsg);
			break;
		}
		msgBuff++;
	}
}

/**
  * @brief
  *
  */
void CANUpdate(void)
{
	CAN_TX_QUEUE_TYPE* txQueueItem;
	while ((txQueueItem = CANTxQueueGet()) != NULL)
	{
		CAN_TX_QUEUE_TYPE txPacket = *txQueueItem;
		uint16_t canId = txPacket.standard_id;

		txPacket.frame_type = CAN_TFT_DATA;
		if (CAN_Send(&txPacket) != ERR_NONE)
		{
			break;
		}
		CANTxQueueShift();
	}

	CAN_RX_QUEUE_TYPE* rxQueueItem;
	while ((rxQueueItem = CANRxQueueGet()) != NULL)
	{
		CO_CANinterrupt_RX(rxQueueItem);
		CANRxQueueShift();
	}
}

/**
  * @brief
  *
  */
bool_t CO_LSSchkBitrateCallback(void* object, uint16_t bitRate)
{
	(void)object;
	return (can_get_bitrate_index(bitRate) >= 0);
}
