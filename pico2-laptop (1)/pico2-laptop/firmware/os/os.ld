ENTRY(Reset_Handler)
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x10040000, LENGTH = 3M
  RAM   (rwx) : ORIGIN = 0x20008000, LENGTH = 0x7A000    /* up to 0x20082000 */
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS
{
  .text : {
    KEEP(*(.vectors))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH
  .data : ALIGN(4) { _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : ALIGN(4) { _sbss = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > RAM
  _heap_start = ALIGN(_ebss, 8);
  _heap_end   = _estack - 4096;               /* top 4 KB = interrupt stack */
  ASSERT(_heap_end > _heap_start + 65536, "OS RAM: heap too small")
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) }
}
