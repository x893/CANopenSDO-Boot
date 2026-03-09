/* Auto-generated GCC startup from MDK vector table */
.syntax unified
.cpu cortex-m4
.thumb

.extern SystemInit
.extern main
.extern __libc_init_array

.extern _sidata
.extern _sdata
.extern _edata
.extern _sbss
.extern _ebss
.extern _estack

.global g_pfnVectors
.global g_pfnVectorsEnd
.global Reset_Handler

.section .isr_vector,"a",%progbits
g_pfnVectors:
  .word _estack
  .word Reset_Handler + 1
  .word NMI_Handler
  .word HardFault_Handler
  .word MemManage_Handler
  .word BusFault_Handler
  .word UsageFault_Handler
  .word 0
  .word 0
  .word 0
  .word 0
  .word SVC_Handler
  .word DebugMon_Handler
  .word 0
  .word PendSV_Handler
  .word SysTick_Handler
  .word WWDT_IRQHandler
  .word PVM_IRQHandler
  .word TAMP_STAMP_IRQHandler
  .word ERTC_WKUP_IRQHandler
  .word FLASH_IRQHandler
  .word CRM_IRQHandler
  .word EXINT0_IRQHandler
  .word EXINT1_IRQHandler
  .word EXINT2_IRQHandler
  .word EXINT3_IRQHandler
  .word EXINT4_IRQHandler
  .word DMA1_Channel1_IRQHandler
  .word DMA1_Channel2_IRQHandler
  .word DMA1_Channel3_IRQHandler
  .word DMA1_Channel4_IRQHandler
  .word DMA1_Channel5_IRQHandler
  .word DMA1_Channel6_IRQHandler
  .word DMA1_Channel7_IRQHandler
  .word ADC1_IRQHandler
  .word CAN1_TX_IRQHandler
  .word CAN1_RX0_IRQHandler
  .word CAN1_RX1_IRQHandler
  .word CAN1_SE_IRQHandler
  .word EXINT9_5_IRQHandler
  .word TMR1_BRK_TMR9_IRQHandler
  .word TMR1_OVF_TMR10_IRQHandler
  .word TMR1_TRG_HALL_TMR11_IRQHandler
  .word TMR1_CH_IRQHandler
  .word TMR2_GLOBAL_IRQHandler
  .word TMR3_GLOBAL_IRQHandler
  .word TMR4_GLOBAL_IRQHandler
  .word I2C1_EVT_IRQHandler
  .word I2C1_ERR_IRQHandler
  .word I2C2_EVT_IRQHandler
  .word I2C2_ERR_IRQHandler
  .word SPI1_IRQHandler
  .word SPI2_IRQHandler
  .word USART1_IRQHandler
  .word USART2_IRQHandler
  .word USART3_IRQHandler
  .word EXINT15_10_IRQHandler
  .word ERTCAlarm_IRQHandler
  .word OTGFS1_WKUP_IRQHandler
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word SDIO1_IRQHandler
  .word TMR5_GLOBAL_IRQHandler
  .word 0
  .word UART4_IRQHandler
  .word UART5_IRQHandler
  .word 0
  .word 0
  .word DMA2_Channel1_IRQHandler
  .word DMA2_Channel2_IRQHandler
  .word DMA2_Channel3_IRQHandler
  .word DMA2_Channel4_5_IRQHandler
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word 0
  .word OTGFS1_IRQHandler
  .word 0
  .word 0
  .word CMP1_IRQHandler
  .word CMP2_IRQHandler
  .word 0
  .word 0
  .word 0
  .word DMA2_Channel6_7_IRQHandler

g_pfnVectorsEnd:

.section .text.Reset_Handler,"ax",%progbits
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
  /* Boot signature check in SRAM (0x20000000) */
  ldr r0, =0x20000000
  ldr r1, [r0]
  ldr r2, =0xDEADBEEF
  cmp r1, r2
  bne normal_boot
  ldr r1, [r0, #4]
  ldr r2, =0xCAFEBEEF
  cmp r1, r2
  bne normal_boot
  /* Clear signature and jump to system bootloader */
  movs r1, #0
  str r1, [r0]
  str r1, [r0, #4]
  ldr r0, =0x1FFFAC00
  ldr r1, [r0]
  ldr r2, [r0, #4]
  msr msp, r1
  bx r2

normal_boot:
  bl SystemInit

  /* Copy .data from FLASH to SRAM */
  ldr r0, =_sidata
  ldr r1, =_sdata
  ldr r2, =_edata
1:
  cmp r1, r2
  ittt lt
  ldrlt r3, [r0], #4
  strlt r3, [r1], #4
  blt 1b

  /* Zero .bss */
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
2:
  cmp r0, r1
  it lt
  strlt r2, [r0], #4
  blt 2b

  bl __libc_init_array
  bl main
  b .

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
.thumb_func
Default_Handler:
  b .

.section .text._init,"ax",%progbits
.thumb_func
.type _init, %function
.global _init
_init:
  bx lr

.section .text._fini,"ax",%progbits
.thumb_func
.type _fini, %function
.global _fini
_fini:
  bx lr


/* Weak aliases for exception handlers */
.weak NMI_Handler
.thumb_set NMI_Handler, Default_Handler
.weak HardFault_Handler
.thumb_set HardFault_Handler, Default_Handler
.weak MemManage_Handler
.thumb_set MemManage_Handler, Default_Handler
.weak BusFault_Handler
.thumb_set BusFault_Handler, Default_Handler
.weak UsageFault_Handler
.thumb_set UsageFault_Handler, Default_Handler
.weak SVC_Handler
.thumb_set SVC_Handler, Default_Handler
.weak DebugMon_Handler
.thumb_set DebugMon_Handler, Default_Handler
.weak PendSV_Handler
.thumb_set PendSV_Handler, Default_Handler
.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler
.weak WWDT_IRQHandler
.thumb_set WWDT_IRQHandler, Default_Handler
.weak PVM_IRQHandler
.thumb_set PVM_IRQHandler, Default_Handler
.weak TAMP_STAMP_IRQHandler
.thumb_set TAMP_STAMP_IRQHandler, Default_Handler
.weak ERTC_WKUP_IRQHandler
.thumb_set ERTC_WKUP_IRQHandler, Default_Handler
.weak FLASH_IRQHandler
.thumb_set FLASH_IRQHandler, Default_Handler
.weak CRM_IRQHandler
.thumb_set CRM_IRQHandler, Default_Handler
.weak EXINT0_IRQHandler
.thumb_set EXINT0_IRQHandler, Default_Handler
.weak EXINT1_IRQHandler
.thumb_set EXINT1_IRQHandler, Default_Handler
.weak EXINT2_IRQHandler
.thumb_set EXINT2_IRQHandler, Default_Handler
.weak EXINT3_IRQHandler
.thumb_set EXINT3_IRQHandler, Default_Handler
.weak EXINT4_IRQHandler
.thumb_set EXINT4_IRQHandler, Default_Handler
.weak DMA1_Channel1_IRQHandler
.thumb_set DMA1_Channel1_IRQHandler, Default_Handler
.weak DMA1_Channel2_IRQHandler
.thumb_set DMA1_Channel2_IRQHandler, Default_Handler
.weak DMA1_Channel3_IRQHandler
.thumb_set DMA1_Channel3_IRQHandler, Default_Handler
.weak DMA1_Channel4_IRQHandler
.thumb_set DMA1_Channel4_IRQHandler, Default_Handler
.weak DMA1_Channel5_IRQHandler
.thumb_set DMA1_Channel5_IRQHandler, Default_Handler
.weak DMA1_Channel6_IRQHandler
.thumb_set DMA1_Channel6_IRQHandler, Default_Handler
.weak DMA1_Channel7_IRQHandler
.thumb_set DMA1_Channel7_IRQHandler, Default_Handler
.weak ADC1_IRQHandler
.thumb_set ADC1_IRQHandler, Default_Handler
.weak CAN1_TX_IRQHandler
.thumb_set CAN1_TX_IRQHandler, Default_Handler
.weak CAN1_RX0_IRQHandler
.thumb_set CAN1_RX0_IRQHandler, Default_Handler
.weak CAN1_RX1_IRQHandler
.thumb_set CAN1_RX1_IRQHandler, Default_Handler
.weak CAN1_SE_IRQHandler
.thumb_set CAN1_SE_IRQHandler, Default_Handler
.weak EXINT9_5_IRQHandler
.thumb_set EXINT9_5_IRQHandler, Default_Handler
.weak TMR1_BRK_TMR9_IRQHandler
.thumb_set TMR1_BRK_TMR9_IRQHandler, Default_Handler
.weak TMR1_OVF_TMR10_IRQHandler
.thumb_set TMR1_OVF_TMR10_IRQHandler, Default_Handler
.weak TMR1_TRG_HALL_TMR11_IRQHandler
.thumb_set TMR1_TRG_HALL_TMR11_IRQHandler, Default_Handler
.weak TMR1_CH_IRQHandler
.thumb_set TMR1_CH_IRQHandler, Default_Handler
.weak TMR2_GLOBAL_IRQHandler
.thumb_set TMR2_GLOBAL_IRQHandler, Default_Handler
.weak TMR3_GLOBAL_IRQHandler
.thumb_set TMR3_GLOBAL_IRQHandler, Default_Handler
.weak TMR4_GLOBAL_IRQHandler
.thumb_set TMR4_GLOBAL_IRQHandler, Default_Handler
.weak I2C1_EVT_IRQHandler
.thumb_set I2C1_EVT_IRQHandler, Default_Handler
.weak I2C1_ERR_IRQHandler
.thumb_set I2C1_ERR_IRQHandler, Default_Handler
.weak I2C2_EVT_IRQHandler
.thumb_set I2C2_EVT_IRQHandler, Default_Handler
.weak I2C2_ERR_IRQHandler
.thumb_set I2C2_ERR_IRQHandler, Default_Handler
.weak SPI1_IRQHandler
.thumb_set SPI1_IRQHandler, Default_Handler
.weak SPI2_IRQHandler
.thumb_set SPI2_IRQHandler, Default_Handler
.weak USART1_IRQHandler
.thumb_set USART1_IRQHandler, Default_Handler
.weak USART2_IRQHandler
.thumb_set USART2_IRQHandler, Default_Handler
.weak USART3_IRQHandler
.thumb_set USART3_IRQHandler, Default_Handler
.weak EXINT15_10_IRQHandler
.thumb_set EXINT15_10_IRQHandler, Default_Handler
.weak ERTCAlarm_IRQHandler
.thumb_set ERTCAlarm_IRQHandler, Default_Handler
.weak OTGFS1_WKUP_IRQHandler
.thumb_set OTGFS1_WKUP_IRQHandler, Default_Handler
.weak SDIO1_IRQHandler
.thumb_set SDIO1_IRQHandler, Default_Handler
.weak TMR5_GLOBAL_IRQHandler
.thumb_set TMR5_GLOBAL_IRQHandler, Default_Handler
.weak UART4_IRQHandler
.thumb_set UART4_IRQHandler, Default_Handler
.weak UART5_IRQHandler
.thumb_set UART5_IRQHandler, Default_Handler
.weak DMA2_Channel1_IRQHandler
.thumb_set DMA2_Channel1_IRQHandler, Default_Handler
.weak DMA2_Channel2_IRQHandler
.thumb_set DMA2_Channel2_IRQHandler, Default_Handler
.weak DMA2_Channel3_IRQHandler
.thumb_set DMA2_Channel3_IRQHandler, Default_Handler
.weak DMA2_Channel4_5_IRQHandler
.thumb_set DMA2_Channel4_5_IRQHandler, Default_Handler
.weak OTGFS1_IRQHandler
.thumb_set OTGFS1_IRQHandler, Default_Handler
.weak CMP1_IRQHandler
.thumb_set CMP1_IRQHandler, Default_Handler
.weak CMP2_IRQHandler
.thumb_set CMP2_IRQHandler, Default_Handler
.weak DMA2_Channel6_7_IRQHandler
.thumb_set DMA2_Channel6_7_IRQHandler, Default_Handler
