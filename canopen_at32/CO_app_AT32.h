#ifndef CANOPENAT32_CO_APP_AT32_H_
#define CANOPENAT32_CO_APP_AT32_H_

#include "CANopen.h"
#include "main.h"
#include "OD.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	/*
	 * Pass in the timer that is going to be used for generating 1ms interrupt for tmrThread function
	 */
	tmr_type* timerHandle;
	/*
	 * Pass in the CAN Handle to this function and it wil be used for all CAN Communications.
	 * It can be FDCan or CAN and CANOpenAT32 Driver will take of care of handling that
	 */
	can_type* CANHandle;
	/*
	 * Pass in the function that initialize the CAN peripheral
	 */
	error_status (*HWInitFunction)(void* cfg);
	CO_t* canOpenStack;

	uint16_t baudrate;		/* This is the baudrate you've set in your CubeMX Configuration */
	uint16_t lssBitrate;	/* LSS configured bitrate (used for LSS pendingBitRate/inquire) */

	uint8_t	desiredNodeID;	/* Desired Node ID */
	uint8_t activeNodeID;	/* Assigned Node ID */

} CANOpenNodeAT32_t;

extern CANOpenNodeAT32_t *canOpenNodeAT32;

int app_canopen_init(void);
void app_program1ms(uint32_t time_interval_ms);
void app_data_init(void);

#define PERIOD_TIMER_4201	10
#define PERIOD_IMPULSE_4203	1000

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
CO_ReturnError_t canopen_storage_init(void);
void canopen_stotage_all(void);
void canopen_stotage_not_all(void);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CANOPENAT32_CO_APP_AT32_H_ */
