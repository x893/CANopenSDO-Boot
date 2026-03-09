#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ================ Device Configuration ================
#ifndef NODE_ID
#define NODE_ID                 0x0A
#endif

#ifndef CAN_BAUDRATE
#define CAN_BAUDRATE            (uint16_t)250000
#endif

#define HEARTBEAT_PRODUCER_MS   1000

// ================ Firmware Loader Configuration ================
#define FIRMWARE_BUFFER_SIZE    0x40000     // 256 KB
#define FIRMWARE_START_ADDRESS   0x08010000
#define FLASH_PAGE_SIZE          0x800       // 2KB

// ================ Object Dictionary Indexes ================
#define OD_INDEX_FIRMWARE       0x2000
#define OD_INDEX_FW_STATUS       0x2001
#define OD_INDEX_FW_COMMAND      0x2002
#define OD_INDEX_FW_CRC         0x2003
#define OD_INDEX_FW_SIZE         0x2004

// ================ LSS Configuration ================
#define LSS_VENDOR_ID           0x00000402  // Artery
#define LSS_PRODUCT_CODE        0x41543332  // 'AT32'
#define LSS_REVISION_NUMBER      0x00010000
#define LSS_SERIAL_NUMBER        0x00000001

// ================ Commands ================
#define FW_CMD_IDLE              0
#define FW_CMD_VERIFY            1
#define FW_CMD_PROGRAM            2
#define FW_CMD_EXECUTE            3
#define FW_CMD_ERASE              4

// ================ Statuses ================
#define FW_STATUS_IDLE            0
#define FW_STATUS_VERIFYING      1
#define FW_STATUS_PROGRAMMING    2
#define FW_STATUS_SUCCESS        3
#define FW_STATUS_ERROR_CRC       4
#define FW_STATUS_ERROR_SIZE      5
#define FW_STATUS_ERROR_VERIFY    6
#define FW_STATUS_ERROR_WRITE     7

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
