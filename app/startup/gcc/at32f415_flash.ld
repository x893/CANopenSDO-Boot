/* AT32F415 linker script for split flash layout (kernel/user app/OD). */
OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(Reset_Handler)

_Min_Heap_Size = 0x0;
_Min_Stack_Size = 0x300;

MEMORY
{
  /* 0x08000000..0x08016FFF: main firmware image (kernel runtime) */
  IROM1 (rx)  : ORIGIN = 0x08000000, LENGTH = 0x017000
  /* 0x08017000..0x0802DFFF: OTA staging slot (reserved, not linked) */
  /* 0x0802E000..0x0802FFFF: kernel call table + helpers */
  IROM2 (rx)  : ORIGIN = 0x0802E000, LENGTH = 0x002000
  /* 0x08030000..0x0803DFFF: user app payload */
  ROM3  (rx)  : ORIGIN = 0x08030000, LENGTH = 0x00E000
  /* 0x0803E000..0x0803EFFF: OD runtime */
  ROM1  (rx)  : ORIGIN = 0x0803E000, LENGTH = 0x001000
  /* 0x0803F000..0x0803FFFF: OD factory */
  ROM2  (rx)  : ORIGIN = 0x0803F000, LENGTH = 0x001000

  IRAM2 (xrw) : ORIGIN = 0x20000000, LENGTH = 0x000170
  IRAM1 (xrw) : ORIGIN = 0x20000170, LENGTH = 0x006E90
  RAM3  (xrw) : ORIGIN = 0x20007000, LENGTH = 0x001000
}

_estack = ORIGIN(IRAM1) + LENGTH(IRAM1);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } > IROM1 =0xFF

  .IROM2 :
  {
    KEEP(*(.KERNEL_CALL_START))
    KEEP(*(.ARM.__at_0x0802E000))
    */kernel_call.o(.text* .rodata* .preinit_array* .init_array*)
  } > IROM2 =0xFF

  .ROM1 :
  {
    KEEP(*(.OD_RUNTIME_START))
    KEEP(*(.ARM.__at_0x0803E000))
    */od_runtime.o(.text* .rodata* .preinit_array* .init_array*)
  } > ROM1 =0xFF

  .ROM2 :
  {
    KEEP(*(.OD_FACTORY_START))
    KEEP(*(.ARM.__at_0x0803F000))
    */od_factory.o(.text* .rodata* .preinit_array* .init_array*)
  } > ROM2 =0xFF

  .ROM3 :
  {
    KEEP(*(.USER_APP_START))
    KEEP(*(.ARM.__at_0x08030000))
    */user_app.o(.text* .rodata* .preinit_array* .init_array*)
    */user-src*.o(.text* .rodata* .preinit_array* .init_array*)
  } > ROM3 =0xFF

  .RAM3 :
  {
    */user_app.o(.data*)
    */user-src*.o(.data*)
  } > RAM3 AT > ROM3

  .RAM3_bss :
  {
    */user_app.o(.bss* COMMON)
    */user-src*.o(.bss* COMMON)
  } > RAM3

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } > IROM1 =0xFF

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } > IROM1 =0xFF

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > IROM1 =0xFF

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } > IROM1 =0xFF

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > IROM1 =0xFF

  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > IROM1 =0xFF

  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > IROM1 =0xFF

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > IRAM1 AT > IROM1

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } > IRAM1

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    _bstack = .;
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > IRAM1

  /DISCARD/ :
  {
    libc.a(*)
    libm.a(*)
    libgcc.a(*)
  }

  .ARM.attributes 0 :
  {
    *(.ARM.attributes)
  }
}
