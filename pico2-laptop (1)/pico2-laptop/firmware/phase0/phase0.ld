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
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH
  .data : ALIGN(4) { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : ALIGN(4) { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
