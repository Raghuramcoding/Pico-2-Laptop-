ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 256K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 32K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
  .text : {
    KEEP(*(.vectors))
    KEEP(*(.embedded_block))
    . = 0x200;                       /* BIOS call table lives at 0x10000200 */
    KEEP(*(.bios_api))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH
  .data : ALIGN(4) { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : ALIGN(4) { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  ASSERT(_ebss <= _estack - 4096, "BIOS RAM too large: no room for its stack")
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
