#include <string.h>
#include <stdlib.h>

#include "boards.h"
#include "CO_driver_target.h"
#include "CO_ODinterface.h"
#include "OD.h"
#include "CO_app_AT32.h"
#include "CO_driver.h"

#define COUNTOF(a)	(sizeof(a) / sizeof(a[0]))

#ifndef COMMIT_ID
#include "version.h"
#endif

ERROR_CODES_t Init_AT32F415(void)
{
	Timer_Init();
	delay_ms(10);
	CAN_RXTX_Init();
	return ERR_NONE;
}

void Board_Set_Serial()
{

#ifndef MCU_ID1
#define         MCU_ID1                   (0x1FFFF7E8)
#define         MCU_ID2                   (0x1FFFF7EC)
#define         MCU_ID3                   (0x1FFFF7F0)
#endif

	OD_RAM.x1018_identity.revisionNumber = COMMIT_ID;

	if (OD_RAM.x1018_identity.serialNumber == 0)
	{
		const uint32_t* Unique_ID = (const uint32_t*)MCU_ID1;
		crc_data_reset();
		OD_RAM.x1018_identity.serialNumber = crc_block_calculate((uint32_t*)Unique_ID, 3);
	}
}

/* delay macros */
#define STEP_DELAY_MS		10

/* delay variable */
static __IO uint32_t fac_us;
static __IO uint32_t fac_ms;
BoardContext_t BoardContext = { 0 };

/**
  * @brief  system clock config program
  * @note   the system clock is configured as follow:
  *         - system clock        = hext / 2 * pll_mult
  *         - system clock source = pll (hext)
  *         - hext                = 8000000
  *         - sclk                = 144000000
  *         - ahbdiv              = 1
  *         - ahbclk              = 144000000
  *         - apb2div             = 2
  *         - apb2clk             = 72000000
  *         - apb1div             = 2
  *         - apb1clk             = 72000000
  *         - pll_mult            = 36
  *         - flash_wtcyc         = 4 cycle
  * @param  none
  * @retval none
  */
void Clock_PreInit(void)
{
	memset(&BoardContext, 0, sizeof(BoardContext));
	nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

	debug_periph_mode_set((0
		| DEBUG_SLEEP
		| DEBUG_DEEPSLEEP
		| DEBUG_STANDBY
		| DEBUG_WDT_PAUSE
		| DEBUG_WWDT_PAUSE
		| DEBUG_I2C1_SMBUS_TIMEOUT
		| DEBUG_I2C2_SMBUS_TIMEOUT
		| DEBUG_TMR1_PAUSE
		| DEBUG_TMR2_PAUSE
		| DEBUG_TMR3_PAUSE
		| DEBUG_TMR4_PAUSE
		| DEBUG_CAN1_PAUSE
		), TRUE
	);

	crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_GPIOF_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_CRC_PERIPH_CLOCK, TRUE);

	gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
}

#define CRM_LICK_STABLE_TIMEOUT	100
#define CRM_HICK_STABLE_TIMEOUT	100
#define CRM_HEXT_STABLE_TIMEOUT	50
#define CRM_PLL_STABLE_TIMEOUT	200
#define CRM_SYSCLK_TIMEOUT		100

/**
  * @brief Initiailize
  */
ERROR_CODES_t Clock_Init(void)
{
#define CLOCK_DONE			100
#define CLOCK_DONE_SET		(CLOCK_DONE - 1)
#define CLOCK_ERROR_SET		10
#define CLOCK_ERROR			(CLOCK_ERROR_SET - 1)
#define CLOCK_HICK			0
#define CLOCK_HEXT			(CLOCK_HICK + 1)
#define CLOCK_PLL			(CLOCK_HEXT + 1)
#define CLOCK_DIV			(CLOCK_PLL + 1)
#define CLOCK_DIV_SET		(CLOCK_DIV - 1)
#define CLOCK_SYS			(CLOCK_DIV + 1)
#define CLOCK_OK			(CLOCK_SYS + 1)

#define CLOCK_HICK_ERROR	(CLOCK_HICK + CLOCK_ERROR_SET)
#define CLOCK_HEXT_ERROR	(CLOCK_HEXT + CLOCK_ERROR_SET)
#define CLOCK_PLL_ERROR		(CLOCK_PLL + CLOCK_ERROR_SET)
#define CLOCK_SYS_ERROR		(CLOCK_SYS + CLOCK_ERROR_SET)

	uint32_t timeout;
	uint16_t state = CLOCK_HICK;

	crm_reset();
	flash_psr_set(FLASH_WAIT_CYCLE_4);
	systick_clock_source_config(SYSTICK_CLOCK_SOURCE_AHBCLK_NODIV);

	while (state < CLOCK_DONE)
	{
		/*
		crm_clock_source_enable(CRM_CLOCK_SOURCE_LICK, TRUE);
		timeout = CRM_LICK_STABLE_TIMEOUT;
		while (timeout != 0 && crm_flag_get(CRM_LICK_STABLE_FLAG) != SET)
			--timeout;
		if (timeout == 0)
			break;
		*/
		switch (state)
		{
		case CLOCK_HICK:
			crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, TRUE);
			timeout = CRM_HICK_STABLE_TIMEOUT;
			while (timeout != 0 && crm_flag_get(CRM_HICK_STABLE_FLAG) != SET)
				--timeout;
			if (timeout == 0)
				state += CLOCK_ERROR;
			break;
		case CLOCK_HEXT:
			crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE);
			timeout = CRM_HEXT_STABLE_TIMEOUT;
			while (timeout != 0 && crm_hext_stable_wait() == ERROR)
				--timeout;
			if (timeout == 0)
				state += CLOCK_ERROR;
			break;
		case CLOCK_PLL:
			crm_pll_config(CRM_PLL_SOURCE_HEXT, CRM_PLL_MULT_18);
			crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);
			timeout = CRM_PLL_STABLE_TIMEOUT;
			while (timeout != 0 && crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)
				--timeout;
			if (timeout == 0)
				state += CLOCK_ERROR;
			break;
		case CLOCK_DIV:
			crm_ahb_div_set(CRM_AHB_DIV_1);
			crm_apb2_div_set(CRM_APB2_DIV_2);
			crm_apb1_div_set(CRM_APB1_DIV_2);
			break;
		case CLOCK_SYS:
			crm_auto_step_mode_enable(TRUE);
			crm_sysclk_switch(CRM_SCLK_PLL);
			timeout = CRM_SYSCLK_TIMEOUT;
			while (timeout != 0 && crm_sysclk_switch_status_get() != CRM_SCLK_PLL)
				--timeout;
			crm_auto_step_mode_enable(FALSE);
			if (timeout == 0)
				state += CLOCK_ERROR;
			break;
		case CLOCK_OK:
			system_core_clock_update();
			delay_init();
			return ERR_NONE;
		
		case CLOCK_HICK_ERROR:
		case CLOCK_PLL_ERROR:
		case CLOCK_SYS_ERROR:
			state = CLOCK_DONE_SET;
			break;
		case CLOCK_HEXT_ERROR:
			crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, FALSE);
			crm_pll_config(CRM_PLL_SOURCE_HICK, CRM_PLL_MULT_36);
			crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);
			timeout = CRM_PLL_STABLE_TIMEOUT;
			while (timeout != 0 && crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)
				--timeout;
			state = (timeout == 0) ? CLOCK_DONE_SET : CLOCK_DIV_SET;
			break;
		}
		++state;
	}
	crm_reset();
	system_core_clock_update();
	delay_init();
	return ERR_BOARD_FAIL;
}

/**
  * @brief  inserts a delay time.
  * @param  nus: specifies the delay time length, in microsecond.
  * @retval none
  */
void delay_us(uint32_t us)
{
	uint32_t temp;
	SysTick->LOAD = (uint32_t)(us * fac_us);
	SysTick->VAL = 0x00;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	do
	{
		temp = SysTick->CTRL;
	} while (
		(temp & SysTick_CTRL_ENABLE_Msk) &&
		!(temp & SysTick_CTRL_COUNTFLAG_Msk)
		);

	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->VAL = 0x00;
}

void delay_init()
{
	fac_us = system_core_clock / (1000000U);
	fac_ms = fac_us * (1000U);
}

/**
  * @brief  inserts a delay time.
  * @param  nms: specifies the delay time length, in milliseconds.
  * @retval none
  */
void delay_ms(uint32_t ms)
{
	volatile uint32_t temp = 0;
	while (ms)
	{
		if (ms > STEP_DELAY_MS)
		{
			SysTick->LOAD = (uint32_t)(STEP_DELAY_MS * fac_ms);
			ms -= STEP_DELAY_MS;
		}
		else
		{
			SysTick->LOAD = (uint32_t)(ms * fac_ms);
			ms = 0;
		}
		SysTick->VAL = 0;
		SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
		do
		{
			temp = SysTick->CTRL;
		} while (
			(temp & SysTick_CTRL_ENABLE_Msk) != 0
			&& (temp & SysTick_CTRL_COUNTFLAG_Msk) == 0
			);

		SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
		SysTick->VAL = 0;
	}
}

void usb_delay_ms(uint32_t ms) __attribute__((alias("delay_ms")));

/**
  * @brief  inserts a delay time.
  * @param  sec: specifies the delay time, in seconds.
  * @retval none
  */
void delay_sec(uint16_t sec)
{
	uint16_t index;
	for (index = 0; index < sec; index++)
	{
		delay_ms(500);
		delay_ms(500);
	}
}

/**
  * @brief  Initialize Timer
  * @param  none
  * @retval error
  */
void Timer_Init(void)
{
	crm_clocks_freq_type crm_clocks_freq_struct = { 0 };

	crm_clocks_freq_get(&crm_clocks_freq_struct);
	crm_periph_clock_enable(CAN_TMR_CRM_CLK, TRUE);

	tmr_reset(CAN_TMR);

	/* time base configuration */
	/* systemclock / 14400 / 10 = 1000 hz */
	tmr_base_init(CAN_TMR, (10 - 1), (crm_clocks_freq_struct.ahb_freq / 10000) - 1);

	tmr_cnt_dir_set(CAN_TMR, TMR_COUNT_UP);

	tmr_clock_source_div_set(CAN_TMR, TMR_CLOCK_DIV1);

	tmr_flag_clear(CAN_TMR, TMR_OVF_FLAG);
	nvic_irq_enable(CAN_TMR_IRQn, 0, 2);

	/* overflow interrupt enable */
	tmr_interrupt_enable(CAN_TMR, TMR_OVF_INT, TRUE);
	tmr_counter_enable(CAN_TMR, TRUE);
}

void Timer_Delay(uint32_t ms)
{
	uint32_t timeout = Timer_GetTicks();
	while ((Timer_GetTicks() - timeout) < ms)
		__WFI();
}

uint32_t Timer_GetTicks(void)
{
	return BoardContext.Timer1ms;
}

/**
  * @brief  this function handles overflow handler.
  * @param  none
  * @retval none
  */
void CAN_TMR_IRQHandler(void)
{
	if (tmr_flag_get(CAN_TMR, TMR_OVF_FLAG) != RESET)
	{
		tmr_flag_clear(CAN_TMR, TMR_OVF_FLAG);
		BoardContext.Timer1ms++;
	}
}

// Calculated for 144MHz System Clock, 72MHz PCLK1 CAN clock
const CO_CANbitRateData_t CO_CANbitRateData[] = {
	{.bitrate = 250,
		.can_baudrate = {
			.baudrate_div = 18,
			.rsaw_size = CAN_RSAW_2TQ,
			.bts1_size = CAN_BTS1_13TQ,
			.bts2_size = CAN_BTS2_2TQ
		}
	},{.bitrate = 125,
		.can_baudrate = {
			.baudrate_div = 36,
			.rsaw_size = CAN_RSAW_2TQ,
			.bts1_size = CAN_BTS1_13TQ,
			.bts2_size = CAN_BTS2_2TQ
		}
	},{.bitrate = 500,
		.can_baudrate = {
			.baudrate_div = 9,
			.rsaw_size = CAN_RSAW_2TQ,
			.bts1_size = CAN_BTS1_13TQ,
			.bts2_size = CAN_BTS2_2TQ
		}
	}
};

/**
  * @brief			Get CAN bitrate parameters table index
  * @param bitRate	Bitrate in kbps
  * @return			Index (0...) or -1
  */
int can_get_bitrate_index(uint16_t bitRate)
{
	if (bitRate > 0)
	{
		for (size_t i = 0; i < COUNTOF(CO_CANbitRateData); i++)
			if (CO_CANbitRateData[i].bitrate == bitRate)
				return i;
	}
	return -1;
}

/**
  *  @brief  can configiguration.
  *  @param  none
  *  @retval none
  */
error_status can_configuration(void* p)
{
	CANOpenNodeAT32_t *CANopenNode = (CANOpenNodeAT32_t *)p;
	error_status err = ERROR;
	can_base_type can_base;
	can_filter_init_type can_filter;

	crm_periph_clock_enable(CAN_CRM_CLK, TRUE);

	/* can base init */
	can_default_para_init(&can_base);
	can_base.mode_selection = CAN_MODE_COMMUNICATE;
	can_base.ttc_enable = FALSE;
	can_base.aebo_enable = TRUE;
	can_base.aed_enable = TRUE;
	can_base.prsf_enable = TRUE;
	can_base.mdrsel_selection = CAN_DISCARDING_FIRST_RECEIVED;
	can_base.mmssr_selection = CAN_SENDING_BY_REQUEST;
	if (can_base_init(CAN_CAN, &can_base) == SUCCESS)
	{
		int index = can_get_bitrate_index(CANopenNode->baudrate);
		if (index < 0) index = 0;
		if (can_baudrate_set(
			CAN_CAN,
			(can_baudrate_type*)&CO_CANbitRateData[index].can_baudrate
		) == SUCCESS)
		{
			/* can filter init */
			can_filter.filter_activate_enable = TRUE;
			can_filter.filter_mode = CAN_FILTER_MODE_ID_MASK;
			can_filter.filter_number = 0;
			can_filter.filter_bit = CAN_FILTER_32BIT;
			can_filter.filter_id_high = 0;
			can_filter.filter_id_low = 0;
			can_filter.filter_mask_high = 0;
			can_filter.filter_mask_low = 0;

			can_filter.filter_fifo = CAN_FILTER_FIFO0;
			can_filter_init(CAN_CAN, &can_filter);

			can_filter.filter_fifo = CAN_FILTER_FIFO1;
			can_filter_init(CAN_CAN, &can_filter);

			/* can interrupt config */
#ifdef CAN_SE_IRQn
			nvic_irq_enable(CAN_SE_IRQn, 0, 2);
			can_interrupt_enable(CAN_CAN, CAN_ETRIEN_INT, TRUE);
			can_interrupt_enable(CAN_CAN, CAN_EOIEN_INT, TRUE);
#endif
#ifdef CAN_TX_IRQn
			nvic_irq_enable(CAN1_TX_IRQn, 0, 2);
			can_interrupt_enable(CAN_CAN, CAN_TCIEN_INT, TRUE);
#endif
#ifdef CAN_RX0_IRQn
			nvic_irq_enable(CAN_RX0_IRQn, 0, 2);
			can_interrupt_enable(CAN_CAN, CAN_RF0MIEN_INT, TRUE);
#endif
#ifdef CAN_RX1_IRQn
			nvic_irq_enable(CAN_RX1_IRQn, 0, 2);
			can_interrupt_enable(CAN_CAN, CAN_RF1MIEN_INT, TRUE);
#endif
			err = SUCCESS;
		}
	}
	return err;
}

#ifdef CAN_RX0_IRQn
/**
  *  @brief  CAN interrupt function RX0
  *  @param  none
  *  @retval none
  */
void CAN_RX0_IRQHandler(void)
{
	static can_rx_message_type msg;
	if (can_flag_get(CAN_CAN, CAN_RF0MN_FLAG) != RESET)
	{
		/* Read received message from FIFO */
		can_message_receive(CAN_CAN, CAN_RX_FIFO0, &msg);
		CANRxQueuePut(&msg);
	}
}
#endif

#ifdef CAN_RX1_IRQn
/**
  *  @brief  CAN interrupt function RX1
  *  @param  none
  *  @retval none
  */
void CAN_RX1_IRQHandler(void)
{
	static can_rx_message_type msg;
	if (can_flag_get(CAN_CAN, CAN_RF1MN_FLAG) != RESET)
	{
		/* Read received message from FIFO */
		can_message_receive(CAN_CAN, CAN_RX_FIFO1, &msg);
		CANRxQueuePut(&msg);
	}
}
#endif

#ifdef CAN_TX_IRQn
void CO_CANinterrupt_TX(void);

/**
  *  @brief  CAN interrupt function TX
  *  @param  none
  *  @retval none
  */
void CAN_TX_IRQHandler(void)
{
	if (can_flag_get(CAN_CAN, CAN_TM0TCF_FLAG) != RESET)
	{
		can_flag_clear(CAN_CAN, CAN_TM0TCF_FLAG);
		CO_CANinterrupt_TX();
	}
	if (can_flag_get(CAN_CAN, CAN_TM1TCF_FLAG) != RESET)
	{
		can_flag_clear(CAN_CAN, CAN_TM1TCF_FLAG);
		CO_CANinterrupt_TX();
	}
	if (can_flag_get(CAN_CAN, CAN_TM2TCF_FLAG) != RESET)
	{
		can_flag_clear(CAN_CAN, CAN_TM2TCF_FLAG);
		CO_CANinterrupt_TX();
	}
}
#endif

#ifdef CAN_SE_IRQn
/**
  *  @brief  CAN interrupt function SE
  *  @param  none
  *  @retval none
  */
void CAN_SE_IRQHandler(void)
{
	if (can_flag_get(CAN_CAN, CAN_ETR_FLAG) != RESET)
	{
		// can_error_type_record_get(CAN_CAN);
		uint32_t err_index = CAN_CAN->ests & 0x70;
		can_flag_clear(CAN_CAN, CAN_ETR_FLAG);

		/* error type is stuff error */
		if (err_index == 0x10) // CAN_ERRORRECORD_STUFFERR)
		{
			CAN_Reset();
			can_configuration(BoardContext.lastCANbaudrate);
			/*
			when stuff error occur:
			in order to ensure communication normally,
			user must restart CAN
			or send a frame of highest priority message here
			*/
		}
		else
		{
			can_transmit_cancel(CAN_CAN, CAN_TX_MAILBOX0);
			can_transmit_cancel(CAN_CAN, CAN_TX_MAILBOX1);
			can_transmit_cancel(CAN_CAN, CAN_TX_MAILBOX2);
		}
	}
}
#endif

void CAN_Reset(void)
{
	can_reset(CAN_CAN);
}

void CAN_RXTX_Init(void)
{
	gpio_init_type gpio;

	gpio_default_para_init(&gpio);

	gpio_pin_remap_config(CAN_IOMUX_SET, TRUE);

	gpio.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
	gpio.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
	gpio.gpio_pull = GPIO_PULL_NONE;
	gpio.gpio_mode = GPIO_MODE_MUX;
	gpio.gpio_pins = CAN_TX_PIN;
	gpio_init(CAN_TX_PORT, &gpio);

	gpio.gpio_pull = GPIO_PULL_UP;
	gpio.gpio_mode = GPIO_MODE_INPUT;
	gpio.gpio_pins = CAN_RX_PIN;
	gpio_init(CAN_RX_PORT, &gpio);
}

/**
  * @brief Send message to controller
  * @return 0 if no free slot, 1 if success
  */
ERROR_CODES_t CAN_Send(can_tx_message_type* msg)
{
	return CAN_TX_STATUS_NO_EMPTY == can_message_transmit(CAN_CAN, msg)
		? ERR_TX_FAIL
		: ERR_NONE;
}

