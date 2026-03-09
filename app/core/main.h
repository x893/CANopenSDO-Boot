#ifndef __MAIN_H__
#define __MAIN_H__

#include "at32f415.h"

#define BOARD_HARDWARE_VERSION	"100"
#define BOARD_SOFTWARE_VERSION	"200"

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum {
		ERR_NONE = 0,
		ERR_OD_INVALID,
		ERR_OD_INIT,
		ERR_BOARD_INVALID,
		ERR_BOARD_FAIL,
		ERR_BAD_PARAMS,
		ERR_TX_FAIL,
		ERR_FATAL,
		ERR_UNCHANGED,
		ERR_CAN_FAIL,
		ERR_NOT_SUPPORTED,
	} ERROR_CODES_t;

	__NO_RETURN
	void ErrorHandler(ERROR_CODES_t errorCode);

#define BUTTON_SHORT_PRESS	1
#define BUTTON_LONG_PRESS	2

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */
