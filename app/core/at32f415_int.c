#include "at32f415_int.h"
#include "CO_app_AT32.h"

void NMI_Handler(void)
{
	// Nothing to do
}
__NO_RETURN void MemManage_Handler(void)
{
	__set_MSP(*((uint32_t*)FLASH_BASE));
	ErrorHandler(ERR_FATAL);
}

__NO_RETURN void BusFault_Handler(void)
{
	__set_MSP(*((uint32_t*)FLASH_BASE));
	ErrorHandler(ERR_FATAL);
}

__NO_RETURN void UsageFault_Handler(void)
{
	__set_MSP(*((uint32_t*)FLASH_BASE));
	ErrorHandler(ERR_FATAL);
}
