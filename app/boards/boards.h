#ifndef __BOARDS_H__
#define __BOARDS_H__

#include <stdint.h>
#include <stdbool.h>

#include "main.h"

#define VECTORS_TABLE_RAM		SRAM_BASE

/*
 * CiA 401 object 0x1000:
 * - low word  = device profile number 0x0191;
 * - high word = implemented channel groups derived from the board profile.
 */
#define CIA401_DEVICE_TYPE_PROFILE_IO	0x0191U
#define CIA401_DEVICE_TYPE_DI			(1UL << 16)
#define CIA401_DEVICE_TYPE_DO			(1UL << 17)
#define CIA401_DEVICE_TYPE_AI			(1UL << 18)
#define CIA401_DEVICE_TYPE_AO			(1UL << 19)

#define CAN_TMR					TMR3
#define CAN_TMR_IRQn			TMR3_GLOBAL_IRQn
#define CAN_TMR_IRQHandler		TMR3_GLOBAL_IRQHandler
#define CAN_TMR_CRM_CLK			CRM_TMR3_PERIPH_CLOCK
#define CAN_TMR_CRM_RESET		CRM_TMR3_PERIPH_RESET

#define CAN_CAN					CAN1

#define CAN_RX0_IRQn			CAN1_RX0_IRQn
#define CAN_RX1_IRQn			CAN1_RX1_IRQn
// #define CAN_TX_IRQn			CAN1_TX_IRQn
// #define CAN_SE_IRQn			CAN1_SE_IRQn

#define CAN_CRM_CLK				CRM_CAN1_PERIPH_CLOCK
#define CAN_CRM_RESET			CRM_CAN1_PERIPH_RESET

#define CAN_TX_IRQHandler		CAN1_TX_IRQHandler
#define CAN_RX0_IRQHandler		CAN1_RX0_IRQHandler
#define CAN_RX1_IRQHandler		CAN1_RX1_IRQHandler
#define CAN_SE_IRQHandler		CAN1_SE_IRQHandler

#define CAN_RX_PIN				GPIO_PINS_8
#define CAN_RX_PORT				GPIOB

#define CAN_TX_PIN				GPIO_PINS_9
#define CAN_TX_PORT				GPIOB

#define CAN_IOMUX_SET			CAN1_GMUX_0010

#ifdef __cplusplus
extern "C" {
#endif

void Board_Set_Serial(void);

ERROR_CODES_t Init_AT32F415(void);
void CAN_RXTX_Init(void);

void Clock_PreInit(void);
ERROR_CODES_t Clock_Init(void);

void Timer_Init(void);

uint32_t Timer_GetTicks(void);

void Timer_Delay(uint32_t ms);

void CAN_Reset(void);
ERROR_CODES_t CAN_Send(can_tx_message_type* msg);

void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);
void delay_sec(uint16_t sec);
error_status can_configuration(void* CANopenNode);

typedef __PACKED_STRUCT {
	uint32_t Timer1ms;
	uint32_t Press_Time;

	uint16_t lastCANbaudrate;

	uint8_t log_enable;
	uint8_t disable_handlers;
} BoardContext_t;

extern BoardContext_t BoardContext;
int can_get_bitrate_index(uint16_t bitRate);

#ifdef USE_WDT
#define WDT_RELOAD()	wdt_reload_value_set(0xFFF)
#define WDT_RESET( )	wdt_counter_reload( )
#define WDT_INIT( )		WDT_Init( )
#else
#define WDT_RELOAD()
#define WDT_RESET( )
#define WDT_INIT( )
#endif

#ifdef __cplusplus
}
#endif

#endif
