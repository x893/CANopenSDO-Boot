#include "boards.h"
#include "CO_app_AT32.h"
#include "OD.h"
#include "version.h"

#include "debug.h"

// static
CANOpenNodeAT32_t localCanOpenNode = {
	.HWInitFunction = can_configuration,
	.timerHandle = CAN_TMR,
	.CANHandle = CAN_CAN
};
CANOpenNodeAT32_t *canOpenNodeAT32 = &localCanOpenNode;

#ifdef USE_WDT
static void WDT_Init(void)
{
	if (crm_flag_get(CRM_WDT_RESET_FLAG) != RESET)
	{
		/* Reset from wdt */
		crm_flag_clear(CRM_WDT_RESET_FLAG);
	}

	wdt_register_write_enable(TRUE); /* disable register write protection */
	wdt_divider_set(WDT_CLK_DIV_32); /* set the wdt divider value */

	/* Set reload value
	timeout = reload_value * (divider / lick_freq )    (s)

	lick_freq    = 40000 Hz
	divider      = 32
	reload_value = 6000

	timeout = 4000 * (32 / 40000 ) = 3.2s = 3200ms
	*/
	wdt_reload_value_set(4000 - 1);
	wdt_counter_reload();
	wdt_enable();
}
#endif

/**	
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
	CO_t* CO;
	CO_NMT_reset_cmd_t reset_status = CO_RESET_NOT;
	uint32_t time_old, time_current, time_interval, time_interval_us;

	Clock_PreInit();
	delay_init();

	if (ERR_NONE != Clock_Init())
		ErrorHandler(ERR_BOARD_FAIL);

	__enable_irq();

	if ((CO = CO_new(NULL, &time_current)) == NULL)
		ErrorHandler(ERR_CAN_FAIL);

	canOpenNodeAT32->canOpenStack = CO;
	canOpenNodeAT32->desiredNodeID = 0x7F;
	canOpenNodeAT32->lssBitrate = 250;
	canOpenNodeAT32->baudrate = canOpenNodeAT32->lssBitrate;

	if (!CO_LSSchkBitrateCallback(NULL, canOpenNodeAT32->lssBitrate))
	{
		canOpenNodeAT32->lssBitrate = 250;
		canOpenNodeAT32->baudrate = 250;
	}
	if (canOpenNodeAT32->desiredNodeID < 1)
		canOpenNodeAT32->desiredNodeID = CO_LSS_NODE_ID_ASSIGNMENT;

	if (ERR_NONE != Init_AT32F415())
		ErrorHandler(ERR_BOARD_FAIL);

	app_data_init();

	WDT_INIT();

	while (reset_status != CO_RESET_APP)
	{
		if (app_canopen_init() != 0)
			ErrorHandler(ERR_CAN_FAIL);

		time_old = Timer_GetTicks();
		reset_status = CO_RESET_NOT;

		while (reset_status == CO_RESET_NOT)
		{
#ifdef USE_DEBUG
			StackCheck();
#endif
			time_current = Timer_GetTicks();
			time_interval = time_current - time_old;
			if (time_interval != 0)
			{
				time_interval_us = time_interval * 1000;
				time_old = time_current;

				WDT_RESET();
				CANUpdate();

				WDT_RESET();
				reset_status = CO_process(CO, false, time_interval_us, NULL);

				WDT_RESET();
				if (reset_status == CO_RESET_COMM)
				{
					CO_CANsetConfigurationMode(canOpenNodeAT32);
					break;
				}
				else if (reset_status == CO_RESET_APP)
				{
					break;
				}

				if ((!CO->nodeIdUnconfigured) && CO->CANmodule->CANnormal)
				{
					WDT_RESET();
					app_program1ms(time_interval);
				}
			}
		}
		tmr_counter_enable(canOpenNodeAT32->timerHandle, FALSE);
	}

	CO_CANsetConfigurationMode(canOpenNodeAT32);
	CO_delete(CO);

	NVIC_SystemReset();
}

__NO_RETURN
void ErrorHandler(ERROR_CODES_t errorCode)
{
	__disable_irq();
	delay_ms(5000);
	NVIC_SystemReset();
}

#if !defined( __ARMCC_VERSION )
#include <sys/reent.h>
int _close(struct _reent *r, int x)
{
	return 0;
}
_off_t _lseek_r(struct _reent *r, int x, _off_t o, int y)
{
	return (_off_t)0;
}
_ssize_t _read_r(struct _reent *r, int x, void *v, size_t s)
{
	return 0;
}
_ssize_t _write_r(struct _reent *r, int x, const void *v, size_t s)
{
	return 0;
}
#endif
