/* Vector table, reset handler and fault printer shared by phase0, BIOS and OS.
 * Build with -DWITH_IMAGE_DEF for images the RP2350 boot ROM must accept
 * (phase0 and the BIOS).  The OS is started by the BIOS, so it has no block. */
#include "rp2350.h"

extern int main(void);
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Default_Handler(void) { for (;;) { } }

#define WEAK_DEFAULT __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void)        WEAK_DEFAULT;
void MemManage_Handler(void)  WEAK_DEFAULT;
void BusFault_Handler(void)   WEAK_DEFAULT;
void UsageFault_Handler(void) WEAK_DEFAULT;
void SecureFault_Handler(void) WEAK_DEFAULT;
void SVC_Handler(void)        WEAK_DEFAULT;
void DebugMon_Handler(void)   WEAK_DEFAULT;
void PendSV_Handler(void)     WEAK_DEFAULT;
void SysTick_Handler(void)    WEAK_DEFAULT;
void HardFault_Handler(void);
void Reset_Handler(void);

typedef void (*isr_t)(void);

__attribute__((section(".vectors"), used, aligned(512)))
const isr_t vector_table[16 + 52] = {
    (isr_t)&_estack, Reset_Handler, NMI_Handler, HardFault_Handler,
    MemManage_Handler, BusFault_Handler, UsageFault_Handler, SecureFault_Handler,
    0, 0, 0, SVC_Handler, DebugMon_Handler, 0, PendSV_Handler, SysTick_Handler,
    [16 ... 67] = Default_Handler
};

#ifdef WITH_IMAGE_DEF
/* Minimal "secure ARM executable for RP2350" block.  The boot ROM looks for
 * this within the first 4 KB of flash; without it the image is ignored. */
__attribute__((section(".embedded_block"), used))
const uint32_t image_def[5] = {
    0xffffded3u,   /* block start marker                       */
    0x10210142u,   /* item: image type = EXE, secure, RP2350    */
    0x000001ffu,   /* item: last item, 1 word of items          */
    0x00000000u,   /* link to next block: none (loops to self)  */
    0xab123579u    /* block end marker                          */
};
#endif

void Reset_Handler(void)
{
    uint32_t *s = &_sidata, *d = &_sdata;
    while (d < &_edata) *d++ = *s++;
    for (uint32_t *b = &_sbss; b < &_ebss; b++) *b = 0;
    REG(SCB_VTOR) = (uint32_t)(uintptr_t)vector_table;
    main();
    for (;;) { }
}

/* ---- fault handling: print the stacked registers on UART0, blink LED ---- */
static void raw_putc(char c)
{
    if (!(REG(UART0_BASE + 0x30) & 1u)) return;          /* UART not enabled */
    while (REG(UART0_BASE + 0x18) & (1u << 5)) { }       /* TX FIFO full     */
    REG(UART0_BASE) = (uint32_t)c;
}
static void raw_puts(const char *s) { while (*s) raw_putc(*s++); }
static void raw_hex(uint32_t v)
{
    for (int i = 28; i >= 0; i -= 4) raw_putc("0123456789abcdef"[(v >> i) & 15]);
}
static void raw_reg(const char *name, uint32_t v)
{
    raw_puts(name); raw_puts("="); raw_hex(v); raw_puts(" ");
}

void fault_c(uint32_t *f)
{
    raw_puts("\r\n*** HARDFAULT ***\r\n");
    raw_reg("pc", f[6]);  raw_reg("lr", f[5]);  raw_reg("psr", f[7]);
    raw_puts("\r\n");
    raw_reg("r0", f[0]);  raw_reg("r1", f[1]);  raw_reg("r2", f[2]);  raw_reg("r3", f[3]);
    raw_puts("\r\n");
    raw_reg("cfsr", REG(SCB_CFSR)); raw_reg("hfsr", REG(SCB_HFSR));
    raw_puts("\r\n");
    REG(RESETS_BASE) &= ~(RST_IO_BANK0 | RST_PADS_BANK0);
    gpio_out_init(PIN_LED, false);
    for (;;) {                       /* very fast blink = hard fault */
        gpio_hi(PIN_LED);
        for (volatile uint32_t i = 0; i < 300000; i++) { }
        gpio_lo(PIN_LED);
        for (volatile uint32_t i = 0; i < 300000; i++) { }
    }
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b fault_c\n");
}
