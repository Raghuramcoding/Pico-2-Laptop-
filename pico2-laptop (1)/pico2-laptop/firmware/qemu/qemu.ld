ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 1M
  RAM   (rwx) : ORIGIN = 0x38000000, LENGTH = 512K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
  .text : {
    KEEP(*(.vectors))
    . = 0x200;
    KEEP(*(.bios_api))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH
  .data : ALIGN(4) { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : ALIGN(4) { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  _heap_start = ALIGN(_ebss, 8);
  _heap_end   = _estack - 4096;
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
