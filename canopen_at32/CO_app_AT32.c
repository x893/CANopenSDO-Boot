#include <stdio.h>

#include "CO_app_AT32.h"
#include "CANopen.h"
#include "boards.h"
#include "storage/CO_storage.h"
#include "OD.h"
#include "CO_PDO.h"
#include "ota_stage.h"
#include "debug.h"

#define USE_AS_U32(a)			(*(uint32_t*)(&a))
#define USE_AS_U16(a)			(*(uint16_t*)(&a))
#define COUNT_OF_ELEMENTS(a)	(sizeof(a) / sizeof(a[0]))

#define NMT_CONTROL	(					\
    CO_NMT_STARTUP_TO_OPERATIONAL	|	\
	CO_ERR_REG_GENERIC_ERR			|	\
	CO_ERR_REG_COMMUNICATION			\
	)
#define FIRST_HB_TIME        500
/*
 * Keep server transfer context alive during OTA segmented download retries:
 * test tooling may wait up to 10s for segment response before retrying.
 */
#define SDO_SRV_TIMEOUT_TIME 15000
#define SDO_CLI_TIMEOUT_TIME 500
#define SDO_CLI_BLOCK        false
#define OD_STATUS_BITS       NULL

#define OTA_CMD_START		0x01
#define OTA_CMD_DATA		0x02
#define OTA_CMD_END			0x03
#define OTA_CMD_ABORT		0x04
#define OTA_CMD_STATUS_REQ	0x05
#define OTA_CMD_APPLY		0x06

#define OTA_TARGET_AT32		0x00

#define OTA_STATE_IDLE		0x00
#define OTA_STATE_RECV		0x01
#define OTA_STATE_VERIFY	0x02
#define OTA_STATE_FLASH		0x03
#define OTA_STATE_REBOOT	0x04
#define OTA_STATE_ERROR		0x05

#define OTA_IMAGE_TYPE_KERNEL	0x00
// #define OTA_IMAGE_TYPE_PLC    0x01

#define OTA_ERR_INVALID_COMMAND 0x0001
#define OTA_ERR_FORWARD_FAILED  0x0002
#define OTA_ERR_INVALID_SIZE    0x0003
#define OTA_ERR_APPLY_UNSUPP    0x0004
#define OTA_ERR_CRC_MISMATCH    0x0005
#define OTA_ERR_INVALID_TARGET  0x0006
#define OTA_ERR_BAD_STATE       0x0007
#define OTA_ERR_INVALID_IMAGE   0x0008
#define OTA_ERR_STAGE_WRITE     0x0009

#define OTA_DIAG_PHASE_NONE          0x00
#define OTA_DIAG_PHASE_CTRL_START    0x10
#define OTA_DIAG_PHASE_CTRL_END      0x11
#define OTA_DIAG_PHASE_CTRL_ABORT    0x12
#define OTA_DIAG_PHASE_CTRL_CHUNK    0x13
#define OTA_DIAG_PHASE_DATA_ENTER    0x20
#define OTA_DIAG_PHASE_DATA_STATE    0x21
#define OTA_DIAG_PHASE_DATA_TARGET   0x22
#define OTA_DIAG_PHASE_DATA_OVERSIZE 0x23
#define OTA_DIAG_PHASE_DATA_RETURN   0x24
#define OTA_DIAG_PHASE_DATA_TRANSPORT 0x25
#define OTA_DIAG_PHASE_DATA_SDO_ABORT 0x26

#define OTA_DIAG_RESULT_NONE    0x00
#define OTA_DIAG_RESULT_OK      0x01
#define OTA_DIAG_RESULT_PARTIAL 0x02
#define OTA_DIAG_RESULT_ERROR   0x03

#define OTA_APPLY_DELAY_MS 200U

/* Global variables and objects */
CO_t* CO = NULL; /* CANopen object */

typedef struct {
	uint32_t ota_segment_index;
	uint32_t ota_segment_completed;
	uint32_t ota_image_crc32;
	uint32_t ota_apply_at32_swap_deadline_ms;
	uint32_t ota_stage_pending_word_addr;
	uint32_t ota_stage_pending_word_data;
	uint32_t ota_stage_next_offset;
	uint8_t ota_stage_pending_word_mask;

	bool ota_data_transfer_session;
	bool ota_apply_at32_swap_pending;
	bool ota_stage_prepared;
	bool ota_stage_pending_word_valid;

} OTA_Context_t;
static OTA_Context_t OTA_Context = {
	.ota_stage_pending_word_data = 0xFFFFFFFFU
};

static void ota_apply_cancel_pending(void);
static void ota_apply_schedule_stage_swap(void);
static void ota_apply_process_pending(void);
static void ota_stage_reset_cache(void);
static bool ota_stage_flush_pending_word(void);

static bool_t LSScfgStoreCallback(void* object, uint8_t id, uint16_t bitRate)
{
	CANOpenNodeAT32_t *node = (CANOpenNodeAT32_t *)object;
	node->lssBitrate = node->baudrate = bitRate;
	node->desiredNodeID = id;
	return true;
}

/*
 *
 *
 * APPLICATION
 *
 *
 */

void app_program1ms(uint32_t ms)
{
	ota_apply_process_pending();
}

/**
  * @brief This function will basically setup the CANopen node
  *
  */
int app_canopen_init()
{
	CO_ReturnError_t err;

	CO = canOpenNodeAT32->canOpenStack;
	CO->CANmodule->CANnormal = false;

	/* Enter CAN configuration. */
	CO_CANsetConfigurationMode((void*)canOpenNodeAT32);
	CO_CANmodule_disable(CO->CANmodule);

	/* initialize CANopen */
	err = CO_CANinit(CO, canOpenNodeAT32, 0);
	if (err != CO_ERROR_NO)
	{
		return 1;
	}

	Board_Set_Serial();
	OD_RAM.x5F03_fwUpdateInfo.bootFwVersion = OD_RAM.x1018_identity.revisionNumber;

	CO_LSS_address_t lssAddress = {
		.identity = {
			.vendorID = OD_RAM.x1018_identity.vendorID,
			.productCode = OD_RAM.x1018_identity.productCode,
			.revisionNumber = OD_RAM.x1018_identity.revisionNumber,
			.serialNumber = OD_RAM.x1018_identity.serialNumber
		}
	};
	err = CO_LSSinit(
		CO,
		&lssAddress,
		&canOpenNodeAT32->desiredNodeID,
		&canOpenNodeAT32->lssBitrate
	);
	if (err != CO_ERROR_NO)
	{
		return 2;
	}

	canOpenNodeAT32->activeNodeID = canOpenNodeAT32->desiredNodeID;
	uint32_t errInfo = 0;

	err = CO_CANopenInit(
		CO,						/* CANopen object */
		NULL,					/* alternate NMT */
		NULL,					/* alternate em */
		OD,						/* Object dictionary */
		OD_STATUS_BITS,			/* Optional OD_statusBits */
        (uint16_t)(NMT_CONTROL),
		FIRST_HB_TIME,
		SDO_SRV_TIMEOUT_TIME,	/* SDOserverTimeoutTime_ms */
		SDO_CLI_TIMEOUT_TIME,	/* SDOclientTimeoutTime_ms */
		SDO_CLI_BLOCK,			/* SDOclientBlockTransfer */
		canOpenNodeAT32->activeNodeID,
		&errInfo);
	if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)
	{
		return 3;
	}

	/* initialize callbacks */
    CO_LSSslave_initCkBitRateCall(CO->LSSslave, NULL, CO_LSSchkBitrateCallback);
    CO_LSSslave_initCfgStoreCall(CO->LSSslave, canOpenNodeAT32, LSScfgStoreCallback);

	if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)
	{
		return 4;
	}

	/* Configure Timer interrupt function for execution every 1 millisecond */
	tmr_counter_enable(canOpenNodeAT32->timerHandle, TRUE);

	/* Configure CAN transmit and receive interrupt */

	CO_CANsetNormalMode(CO->CANmodule);

	return 0;
}

/* Large synthetic domain register (0x2200) for stress read/write over segmented SDO.
 * Payload pattern is deterministic: byte[offset] == (offset & 0xFF).
 * It allows large transfer validation without allocating a large RAM buffer. */
#define DOMAIN_UPLOAD_MAX_SIZE     ((OD_size_t)(128U * 1024U))
#define DOMAIN_UPLOAD_DEFAULT_SIZE ((OD_size_t)(64U * 1024U))

/* Current logical size exposed by reads; updated after successful complete writes. */
static OD_size_t dataSize = DOMAIN_UPLOAD_DEFAULT_SIZE;

static uint8_t domain_pattern_byte(OD_size_t offset)
{
	return (uint8_t)(offset & 0xFFU);
}

static ODR_t OD_read_domainUpload(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead)
{
	if (stream == NULL || buf == NULL || countRead == NULL || stream->subIndex != 0)
		return ODR_DEV_INCOMPAT;

	/* Data is generated on demand by offset, so read works for large sizes
	 * without an allocated payload buffer. */
	if (stream->dataOffset == 0)
	{
		stream->dataLength = dataSize;
	}

	if (stream->dataOffset >= dataSize)
	{
		*countRead = 0;
		stream->dataOffset = 0;
		return ODR_OK;
	}

	OD_size_t remaining = dataSize - stream->dataOffset;
	OD_size_t toRead = (count < remaining) ? count : remaining;
	register uint8_t* bufU8 = (uint8_t*)buf;
	OD_size_t i;
	for (i = 0; i < toRead; i++)
	{
		bufU8[i] = domain_pattern_byte(stream->dataOffset + i);
	}
	*countRead = toRead;
	stream->dataOffset += toRead;

	if (stream->dataOffset >= dataSize)
	{
		stream->dataOffset = 0;
		return ODR_OK;
	}

	return ODR_PARTIAL;
}

/*
 * Custom function for reading OD object _domainUpload_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_write_domainUpload(OD_stream_t* stream, const void* buf, OD_size_t count, OD_size_t* countWritten)
{
	if (stream == NULL || buf == NULL || countWritten == NULL || stream->subIndex != 0)
		return ODR_DEV_INCOMPAT;

	if (stream->dataOffset > DOMAIN_UPLOAD_MAX_SIZE || count > (DOMAIN_UPLOAD_MAX_SIZE - stream->dataOffset))
		return ODR_DATA_LONG;

	register const uint8_t* bufU8 = (const uint8_t*)buf;
	OD_size_t i;
	for (i = 0; i < count; i++)
	{
		if (bufU8[i] != domain_pattern_byte(stream->dataOffset + i))
		{
			return ODR_INVALID_VALUE;
		}
	}
	*countWritten = count;
	stream->dataOffset += count;

	if (stream->dataLength > 0 && stream->dataOffset >= stream->dataLength)
	{
		if (stream->dataOffset > DOMAIN_UPLOAD_MAX_SIZE)
			return ODR_DATA_LONG;

		dataSize = stream->dataOffset;
		stream->dataOffset = 0;
		return ODR_OK;
	}

	return ODR_PARTIAL;
}

static OD_extension_t domainUpload_extension = {
	.object = NULL,
	.read = OD_read_domainUpload,
	.write = OD_write_domainUpload
};

static void ota_diag_reset(void)
{
	OD_RAM.x5F03_fwUpdateInfo.mainFwVersion = 0x00000000;
	OD_RAM.x5F03_fwUpdateInfo.commFwVersion = 0x00000000;
	OD_RAM.x5F03_fwUpdateInfo.userFwVersion = 0x00000000;
	OD_RAM.x5F03_fwUpdateInfo.ODVersion = 0x00000000;
}

static void ota_apply_cancel_pending(void)
{
	OTA_Context.ota_apply_at32_swap_pending = false;
	OTA_Context.ota_apply_at32_swap_deadline_ms = 0;
}

static void ota_stage_reset_cache(void)
{
	register OTA_Context_t *ctx = &OTA_Context;
	
	ctx->ota_stage_pending_word_valid = false;
	ctx->ota_stage_pending_word_addr = 0;
	ctx->ota_stage_pending_word_data = 0xFFFFFFFFU;
	ctx->ota_stage_next_offset = 0;
	ctx->ota_stage_pending_word_mask = 0;
}

static bool ota_stage_flush_pending_word(void)
{
	register OTA_Context_t *ctx = &OTA_Context;
	flash_status_type status = FLASH_OPERATE_DONE;

	if (!ctx->ota_stage_pending_word_valid || ctx->ota_stage_pending_word_mask == 0U)
		return true;
#if USE_FLASH
	flash_unlock();
	status = flash_word_program(ctx->ota_stage_pending_word_addr, ctx->ota_stage_pending_word_data);
	flash_lock();
#endif

	if (status != FLASH_OPERATE_DONE)
		return false;

	ctx->ota_stage_pending_word_valid = false;
	ctx->ota_stage_pending_word_addr = 0;
	ctx->ota_stage_pending_word_data = 0xFFFFFFFFU;
	ctx->ota_stage_pending_word_mask = 0;
	return true;
}

static bool ota_stage_prepare(uint32_t imageSize)
{
	uint32_t eraseLen;
	uint32_t addr;

	if (imageSize == 0U || imageSize > OTA_KERNEL_MAX_IMAGE_SIZE)
		return false;
	if ((OTA_FLASH_BASE_ADDR + imageSize) > OTA_STAGE_REGION_START)
		return false;

	eraseLen = (imageSize + OTA_FLASH_PAGE_SIZE_BYTES - 1U) & ~(OTA_FLASH_PAGE_SIZE_BYTES - 1U);
	OTA_Context.ota_stage_prepared = false;
	ota_stage_reset_cache();

	flash_unlock();
	if (flash_operation_wait_for(ERASE_TIMEOUT) != FLASH_OPERATE_DONE)
	{
		flash_lock();
		return false;
	}

#ifdef USE_FLASH
	for (
		addr = OTA_STAGE_REGION_START;
		addr < (OTA_STAGE_REGION_START + eraseLen);
		addr += OTA_FLASH_PAGE_SIZE_BYTES
		)
	{
		if (flash_sector_erase(addr) != FLASH_OPERATE_DONE)
		{
			flash_lock();
			return false;
		}
	}
#endif

	flash_lock();

	OTA_Context.ota_stage_prepared = true;
	return true;
}

static bool ota_stage_write_chunk(uint32_t offset, const uint8_t *data, OD_size_t len)
{
	register OTA_Context_t *ctx = &OTA_Context;
	register uint32_t idx;

	if (!ctx->ota_stage_prepared || data == NULL)
		return false;
	if ((uint32_t)len == 0U)
		return true;
	if (offset >= OTA_KERNEL_MAX_IMAGE_SIZE || ((uint32_t)len > (OTA_KERNEL_MAX_IMAGE_SIZE - offset)))
		return false;
	if (offset != ctx->ota_stage_next_offset)
		return false;

	for (idx = 0U; idx < (uint32_t)len; ++idx)
	{
		uint32_t absoluteOffset = offset + idx;
		uint32_t wordAddr = OTA_STAGE_REGION_START + (absoluteOffset & ~0x3U);
		uint8_t byteInWord = (uint8_t)(absoluteOffset & 0x3U);
		uint32_t shift = ((uint32_t)byteInWord) << 3;

		if (ctx->ota_stage_pending_word_valid && ctx->ota_stage_pending_word_addr != wordAddr)
		{
			if (!ota_stage_flush_pending_word())
				return false;
		}

		if (!ctx->ota_stage_pending_word_valid)
		{
			ctx->ota_stage_pending_word_valid = true;
			ctx->ota_stage_pending_word_addr = wordAddr;
			ctx->ota_stage_pending_word_data = 0xFFFFFFFFU;
			ctx->ota_stage_pending_word_mask = 0U;
		}

		ctx->ota_stage_pending_word_data &= ~(0xFFUL << shift);
		ctx->ota_stage_pending_word_data |= ((uint32_t)data[idx]) << shift;
		ctx->ota_stage_pending_word_mask |= (uint8_t)(1U << byteInWord);

		if (ctx->ota_stage_pending_word_mask == 0x0FU)
		{
			if (!ota_stage_flush_pending_word())
				return false;
		}
	}

	ctx->ota_stage_next_offset = offset + (uint32_t)len;
	return true;
}

static void ota_apply_schedule_stage_swap(void)
{
	OTA_Context.ota_apply_at32_swap_pending = true;
	OTA_Context.ota_apply_at32_swap_deadline_ms = Timer_GetTicks() + OTA_APPLY_DELAY_MS;
}

__WEAK void OtaBoot_ApplyFromStage(uint32_t imageSize, uint32_t expectedCrc32)
{

}

static void ota_apply_process_pending(void)
{
	uint32_t now;

	if (!OTA_Context.ota_apply_at32_swap_pending)
		return;

	now = Timer_GetTicks();
	if ((int32_t)(now - OTA_Context.ota_apply_at32_swap_deadline_ms) < 0)
		return;

	ota_apply_cancel_pending();
	OtaBoot_ApplyFromStage(
		OD_RAM.x5F01_fwUpdateStatus.bytesWritten,
		OD_RAM.x5F01_fwUpdateStatus.imageCRC32
	);
}

static void ota_reset_status(void)
{
	OD_RAM.x5F01_fwUpdateStatus.state = OTA_STATE_IDLE;
	OD_RAM.x5F01_fwUpdateStatus.lastError = 0x0000;
	OD_RAM.x5F01_fwUpdateStatus.bytesReceived = 0x00000000;
	OD_RAM.x5F01_fwUpdateStatus.bytesWritten = 0x00000000;
	OD_RAM.x5F01_fwUpdateStatus.imageCRC32 = 0x00000000;
	OD_RAM.x5F01_fwUpdateStatus.progress = 0x00;
	
	register OTA_Context_t *ctx = &OTA_Context;
	
	ctx->ota_data_transfer_session = false;
	ctx->ota_segment_index = 0;
	ctx->ota_segment_completed = 0;
	ctx->ota_image_crc32 = 0;
	ctx->ota_stage_prepared = false;
	ota_stage_reset_cache();
	ota_apply_cancel_pending();
	ota_diag_reset();
}

static void ota_update_progress(void)
{
	uint32_t expected = OD_RAM.x5F00_fwUpdateControl.expectedSize;
	if (expected > 0)
	{
		uint32_t recv = OD_RAM.x5F01_fwUpdateStatus.bytesReceived;
		uint32_t pct = (recv >= expected) ? 100U : (recv * 100U) / expected;
		OD_RAM.x5F01_fwUpdateStatus.progress = (uint8_t)pct;
	}
}

static void ota_set_error(uint16_t code)
{
	OD_RAM.x5F01_fwUpdateStatus.state = OTA_STATE_ERROR;
	OD_RAM.x5F01_fwUpdateStatus.lastError = code;
}

static void ota_set_success(void)
{
	OD_RAM.x5F01_fwUpdateStatus.lastError = 0x0000;
	OD_RAM.x5F01_fwUpdateStatus.progress = 100;
	OD_RAM.x5F01_fwUpdateStatus.state = OTA_STATE_REBOOT;
	OTA_Context.ota_data_transfer_session = false;
}

static uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
	size_t i;
	uint32_t value = ~crc;

	for (i = 0; i < len; ++i)
	{
		uint32_t current = value ^ data[i];
		uint8_t bit;
		for (bit = 0; bit < 8; ++bit)
		{
			if ((current & 1U) != 0U)
				current = (current >> 1) ^ 0xEDB88320U;
			else
				current >>= 1;
		}
		value = current;
	}
	return ~value;
}

static bool ota_verify_received_image(void)
{
	register OTA_Context_t *ctx = &OTA_Context;

	const uint32_t expected_size = OD_RAM.x5F00_fwUpdateControl.expectedSize;
	const uint32_t expected_crc = OD_RAM.x5F00_fwUpdateControl.expectedCRC32;
	const uint32_t received = OD_RAM.x5F01_fwUpdateStatus.bytesReceived;

	if (expected_size != 0 && received != expected_size)
	{
		ota_set_error(OTA_ERR_INVALID_SIZE);
		return false;
	}

	OD_RAM.x5F01_fwUpdateStatus.imageCRC32 = ctx->ota_image_crc32;
	if (expected_crc != 0 && ctx->ota_image_crc32 != expected_crc)
	{
		ota_set_error(OTA_ERR_CRC_MISMATCH);
		return false;
	}

	return true;
}

static ODR_t OD_write_fwUpdateControl(OD_stream_t *stream, const void *buf,
							OD_size_t count, OD_size_t *countWritten)
{
	ODR_t result = OD_writeOriginal(stream, buf, count, countWritten);
	if (result != ODR_OK)
		return result;
	if (stream == NULL)
		return ODR_DEV_INCOMPAT;
	if (stream->subIndex == 7)
	{
		return result;
	}
	if (stream->subIndex != 1)
		return result;

	switch (OD_RAM.x5F00_fwUpdateControl.command)
	{
		case OTA_CMD_START:
			if (OD_RAM.x5F00_fwUpdateControl.target != OTA_TARGET_AT32)
			{
				ota_set_error(OTA_ERR_INVALID_TARGET);
				return ODR_INVALID_VALUE;
			}
			if (OD_RAM.x5F00_fwUpdateControl.target == OTA_TARGET_AT32 &&
				OD_RAM.x5F00_fwUpdateControl.imageType != OTA_IMAGE_TYPE_KERNEL)
			{
				ota_set_error(OTA_ERR_INVALID_IMAGE);
				return ODR_INVALID_VALUE;
			}
			ota_reset_status();
			OTA_Context.ota_data_transfer_session = true;
			if (OD_RAM.x5F00_fwUpdateControl.target == OTA_TARGET_AT32)
			{
				if (!ota_stage_prepare(OD_RAM.x5F00_fwUpdateControl.expectedSize))
				{
					ota_set_error(OTA_ERR_INVALID_SIZE);
					return ODR_INVALID_VALUE;
				}
			}
			OD_RAM.x5F01_fwUpdateStatus.state = OTA_STATE_RECV;
			break;
		case OTA_CMD_END:
			if (OD_RAM.x5F01_fwUpdateStatus.state != OTA_STATE_RECV)
			{
				ota_set_error(OTA_ERR_BAD_STATE);
				return ODR_INVALID_VALUE;
			}
			if (OD_RAM.x5F00_fwUpdateControl.target == OTA_TARGET_AT32)
			{
				if (!ota_stage_flush_pending_word())
				{
					ota_set_error(OTA_ERR_STAGE_WRITE);
					return ODR_HW;
				}
			}
			OD_RAM.x5F01_fwUpdateStatus.state = OTA_STATE_VERIFY;
			if (!ota_verify_received_image())
				return ODR_INVALID_VALUE;
			if (OD_RAM.x5F00_fwUpdateControl.target != OTA_TARGET_AT32)
			{
				ota_set_error(OTA_ERR_INVALID_TARGET);
				return ODR_INVALID_VALUE;
			}
			OD_RAM.x5F01_fwUpdateStatus.state = OTA_STATE_FLASH;
			/* For both targets we count accepted payload as written.
			 * Actual low-level apply/reboot is board-specific and can be triggered separately. */
			OD_RAM.x5F01_fwUpdateStatus.bytesWritten = OD_RAM.x5F01_fwUpdateStatus.bytesReceived;
			ota_update_progress();
			ota_set_success();
			break;
		case OTA_CMD_ABORT:
			ota_reset_status();
			break;
		case OTA_CMD_STATUS_REQ:
			break;
		case OTA_CMD_APPLY:
			if (OD_RAM.x5F00_fwUpdateControl.target != OTA_TARGET_AT32)
			{
				ota_set_error(OTA_ERR_APPLY_UNSUPP);
				return ODR_INVALID_VALUE;
			}
			if (OD_RAM.x5F00_fwUpdateControl.imageType != OTA_IMAGE_TYPE_KERNEL)
			{
				ota_set_error(OTA_ERR_INVALID_IMAGE);
				return ODR_INVALID_VALUE;
			}
			if (OD_RAM.x5F01_fwUpdateStatus.state != OTA_STATE_REBOOT ||
				OD_RAM.x5F01_fwUpdateStatus.lastError != 0 ||
				OD_RAM.x5F01_fwUpdateStatus.bytesReceived == 0 ||
				OD_RAM.x5F01_fwUpdateStatus.bytesReceived != OD_RAM.x5F01_fwUpdateStatus.bytesWritten
				)
			{
				ota_set_error(OTA_ERR_BAD_STATE);
				return ODR_INVALID_VALUE;
			}
			ota_apply_schedule_stage_swap();
			break;
		default:
			ota_set_error(OTA_ERR_INVALID_COMMAND);
			return ODR_INVALID_VALUE;
	}
	return result;
}

static ODR_t OD_read_fwUpdateData(OD_stream_t *stream, void *buf,
							OD_size_t count, OD_size_t *countRead)
{
	(void)stream;
	(void)buf;
	(void)count;
	if (countRead != NULL)
		*countRead = 0;
	return ODR_WRITEONLY;
}

static ODR_t OD_write_fwUpdateData(OD_stream_t *stream, const void *buf,
							OD_size_t count, OD_size_t *countWritten)
{
	register OTA_Context_t *ctx = &OTA_Context;
	
	uint32_t before_offset;
	uint32_t expected = 0;

	if (stream == NULL || buf == NULL || countWritten == NULL || stream->subIndex != 0)
		return ODR_DEV_INCOMPAT;
	before_offset = stream->dataOffset;
	ctx->ota_segment_index++;

	if (ctx->ota_data_transfer_session)
	{
		stream->dataOffset = 0;
		ctx->ota_data_transfer_session = false;
	}

	if (OD_RAM.x5F01_fwUpdateStatus.state != OTA_STATE_RECV)
	{
		ctx->ota_segment_completed = ctx->ota_segment_index;
		ota_set_error(OTA_ERR_BAD_STATE);
		return ODR_INVALID_VALUE;
	}

	if (stream->dataOffset == 0)
	{
		/* First segment */
		OD_RAM.x5F01_fwUpdateStatus.bytesReceived = 0;
		OD_RAM.x5F01_fwUpdateStatus.bytesWritten = 0;
		ctx->ota_image_crc32 = 0x00000000;
		OD_RAM.x5F01_fwUpdateStatus.imageCRC32 = 0x00000000;
	}

	*countWritten = count;
	OD_RAM.x5F01_fwUpdateStatus.bytesReceived = stream->dataOffset + count;
	ctx->ota_image_crc32 = ota_crc32_update(ctx->ota_image_crc32, (const uint8_t *)buf, (size_t)count);
	OD_RAM.x5F01_fwUpdateStatus.imageCRC32 = ctx->ota_image_crc32;

	if (OD_RAM.x5F00_fwUpdateControl.target == OTA_TARGET_AT32)
	{
		if (!ota_stage_write_chunk(stream->dataOffset, (const uint8_t *)buf, count))
		{
			ctx->ota_segment_completed = ctx->ota_segment_index;
			ota_set_error(OTA_ERR_STAGE_WRITE);
			return ODR_HW;
		}
		OD_RAM.x5F01_fwUpdateStatus.bytesWritten = OD_RAM.x5F01_fwUpdateStatus.bytesReceived;
	}
	else
	{
		ctx->ota_segment_completed = ctx->ota_segment_index;
		ota_set_error(OTA_ERR_INVALID_TARGET);
		return ODR_INVALID_VALUE;
	}
	ota_update_progress();

	stream->dataOffset += count;
	expected = OD_RAM.x5F00_fwUpdateControl.expectedSize;

	if (expected != 0)
	{
		if (stream->dataOffset > expected)
		{
			ota_set_error(OTA_ERR_INVALID_SIZE);
			ctx->ota_segment_completed = ctx->ota_segment_index;
			return ODR_INVALID_VALUE;
		}
		if (stream->dataOffset >= expected)
		{
			ctx->ota_segment_completed = ctx->ota_segment_index;
			return ODR_OK;
		}
		ctx->ota_segment_completed = ctx->ota_segment_index;
		return ODR_PARTIAL;
	}

	if (stream->dataLength > 0 && stream->dataOffset >= stream->dataLength)
	{
		ctx->ota_segment_completed = ctx->ota_segment_index;
		return ODR_OK;
	}
	ctx->ota_segment_completed = ctx->ota_segment_index;
	return ODR_PARTIAL;
}

static OD_extension_t ota_control_extension = {
	.object = NULL,
	.read = OD_readOriginal,
	.write = OD_write_fwUpdateControl
};

static OD_extension_t ota_data_extension = {
	.object = NULL,
	.read = OD_read_fwUpdateData,
	.write = OD_write_fwUpdateData
};

/**
  *
  */
void app_data_init()
{
	/* Setup extension and flags for triggering TPDO. */
	if (
		ODR_OK != OD_extension_init(OD_ENTRY_H5F00_fwUpdateControl, &ota_control_extension) ||
		ODR_OK != OD_extension_init(OD_ENTRY_H5F02_fwUpdateData, &ota_data_extension)
		)
	{
		ErrorHandler(ERR_OD_INVALID);
	}
}
