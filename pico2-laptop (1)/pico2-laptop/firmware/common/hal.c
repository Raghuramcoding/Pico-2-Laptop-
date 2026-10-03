#include "hal.h"

#define CLK_REF_CTRL      (CLOCKS_BASE + 0x30u)
#define CLK_REF_DIV       (CLOCKS_BASE + 0x34u)
#define CLK_REF_SELECTED  (CLOCKS_BASE + 0x38u)
#define CLK_SYS_CTRL      (CLOCKS_BASE + 0x3cu)
#define CLK_SYS_DIV       (CLOCKS_BASE + 0x40u)
#define CLK_SYS_SELECTED  (CLOCKS_BASE + 0x44u)
#define CLK_PERI_CTRL     (CLOCKS_BASE + 0x48u)

bool reset_release(uint32_t mask)
{
    REG(RESETS_BASE) &= ~mask;
    for (uint32_t t = 0; t < 1000000u; t++)
        if ((REG(RESETS_BASE + 8u) & mask) == mask) return true;
    return false;
}

void fail_blink(int code)
{
    REG(RESETS_BASE) &= ~(RST_IO_BANK0 | RST_PADS_BANK0);   /* LED pin needs these */
    for (volatile uint32_t k = 0; k < 20000; k++) { }
    gpio_out_init(PIN_LED, false);
    for (;;) {
        for (int i = 0; i < code; i++) {
            gpio_hi(PIN_LED);
            for (volatile uint32_t k = 0; k < 400000; k++) { }
            gpio_lo(PIN_LED);
            for (volatile uint32_t k = 0; k < 400000; k++) { }
        }
        for (volatile uint32_t k = 0; k < 2500000; k++) { }
    }
}

/* 12 MHz crystal -> PLL 1500 MHz VCO / 5 / 2 = 150 MHz clk_sys */
int clocks_init(void)
{
    uint32_t t;

    /* crystal oscillator */
    REG(XOSC_BASE + 0x00) = 0xaa0u;                     /* 1-15 MHz range */
    REG(XOSC_BASE + 0x0c) = 47u;                        /* startup delay  */
    REG(XOSC_BASE + 0x00) = (0xfabu << 12) | 0xaa0u;    /* enable         */
    for (t = 0; t < 2000000u; t++)
        if (REG(XOSC_BASE + 0x04) & (1u << 31)) break;
    if (t == 2000000u) return FAIL_XOSC;

    /* clk_ref <- xosc */
    REG(CLK_REF_CTRL) = (REG(CLK_REF_CTRL) & ~3u) | 2u;
    for (t = 0; t < 1000000u && !(REG(CLK_REF_SELECTED) & 4u); t++) { }
    if (t == 1000000u) return FAIL_CLKSEL;
    REG(CLK_REF_DIV) = 1u << 16;

    /* clk_sys <- clk_ref while the PLL starts */
    REG(CLK_SYS_CTRL) &= ~1u;
    for (t = 0; t < 1000000u && !(REG(CLK_SYS_SELECTED) & 1u); t++) { }
    if (t == 1000000u) return FAIL_CLKSEL;

    /* PLL */
    if (!reset_release(RST_PLL_SYS)) return FAIL_RESET;
    REG(PLL_SYS_BASE + 0x00) = 1u;                      /* REFDIV = 1    */
    REG(PLL_SYS_BASE + 0x08) = 125u;                    /* FBDIV = 125   */
    REG(PLL_SYS_BASE + 0x04) = (1u << 2) | (1u << 3);   /* VCO on, postdiv off */
    for (t = 0; t < 1000000u; t++)
        if (REG(PLL_SYS_BASE + 0x00) & (1u << 31)) break;   /* LOCK */
    if (t == 1000000u) return FAIL_PLL;
    REG(PLL_SYS_BASE + 0x0c) = (5u << 16) | (2u << 12); /* /5 /2         */
    REG(PLL_SYS_BASE + 0x04) = (1u << 2);               /* postdiv on    */

    /* clk_sys <- PLL */
    REG(CLK_SYS_CTRL) &= ~(7u << 5);                    /* aux = pll_sys */
    REG(CLK_SYS_DIV) = 1u << 16;
    REG(CLK_SYS_CTRL) |= 1u;                            /* use aux       */
    for (t = 0; t < 1000000u && !(REG(CLK_SYS_SELECTED) & 2u); t++) { }
    if (t == 1000000u) return FAIL_CLKSEL;

    /* clk_peri <- clk_sys */
    REG(CLK_PERI_CTRL) = (1u << 11);
    return 0;
}

/* ---- timer: 1 MHz counter ---- */
void timer_init(void)
{
    if (!reset_release(RST_TIMER0)) fail_blink(FAIL_RESET);
    REG(TICKS_BASE + 0x1c) = XOSC_HZ / 1000000u;        /* TIMER0 cycles */
    REG(TICKS_BASE + 0x18) = 1u;                        /* TIMER0 enable */
}
uint32_t micros(void) { return REG(TIMER0_BASE + 0x28); }
uint32_t millis(void)
{
    uint32_t hi, lo, hi2;
    do {
        hi = REG(TIMER0_BASE + 0x24);
        lo = REG(TIMER0_BASE + 0x28);
        hi2 = REG(TIMER0_BASE + 0x24);
    } while (hi != hi2);
    uint64_t us = ((uint64_t)hi << 32) | lo;
    return (uint32_t)(us / 1000u);
}
void delay_us(uint32_t us)
{
    uint32_t s = micros();
    while ((uint32_t)(micros() - s) < us) { }
}
void delay_ms(uint32_t ms) { while (ms--) delay_us(1000); }

/* ---- UART0 on GP0 (TX) / GP1 (RX) ---- */
void uart_init(uint32_t baud)
{
    if (!reset_release(RST_IO_BANK0 | RST_PADS_BANK0 | RST_UART0)) fail_blink(FAIL_RESET);
    gpio_config(0, FN_UART, PAD_IE | PAD_SCHMITT);
    gpio_config(1, FN_UART, PAD_IE | PAD_SCHMITT | PAD_PUE);
    uint32_t div = (8u * CPU_HZ) / baud;
    uint32_t ibrd = div >> 7, fbrd;
    if (ibrd == 0) { ibrd = 1; fbrd = 0; }
    else fbrd = ((div & 0x7fu) + 1u) / 2u;
    REG(UART0_BASE + 0x24) = ibrd;
    REG(UART0_BASE + 0x28) = fbrd;
    REG(UART0_BASE + 0x2c) = (3u << 5) | (1u << 4);     /* 8N1, FIFO on */
    REG(UART0_BASE + 0x30) = 1u | (1u << 8) | (1u << 9);/* UART, TX, RX */
}
void uart_putc(char c)
{
    while (REG(UART0_BASE + 0x18) & (1u << 5)) { }
    REG(UART0_BASE) = (uint32_t)(uint8_t)c;
}
int uart_getc(void)
{
    if (REG(UART0_BASE + 0x18) & (1u << 4)) return -1;
    return (int)(REG(UART0_BASE) & 0xffu);
}
void uart_flush(void) { while (REG(UART0_BASE + 0x18) & (1u << 3)) { } }

/* ---- SPI0 (PL022), chip selects are plain GPIOs handled by the drivers ---- */
#define SPI_CR0  (SPI0_BASE + 0x00u)
#define SPI_CR1  (SPI0_BASE + 0x04u)
#define SPI_DR   (SPI0_BASE + 0x08u)
#define SPI_SR   (SPI0_BASE + 0x0cu)
#define SPI_CPSR (SPI0_BASE + 0x10u)

void spi0_init(void)
{
    if (!reset_release(RST_IO_BANK0 | RST_PADS_BANK0 | RST_SPI0)) fail_blink(FAIL_RESET);
    gpio_config(PIN_SPI_SCK,  FN_SPI, PAD_IE | PAD_SCHMITT | PAD_SLEWFAST | PAD_DRIVE_8);
    gpio_config(PIN_SPI_MOSI, FN_SPI, PAD_IE | PAD_SCHMITT | PAD_SLEWFAST | PAD_DRIVE_8);
    gpio_config(PIN_SPI_MISO, FN_SPI, PAD_IE | PAD_SCHMITT | PAD_PUE);
    REG(SPI_CR1) = 0;
    REG(SPI_CR0) = 7u;                 /* 8 bit, mode 0 */
    REG(SPI_CPSR) = 254u;
    REG(SPI_CR1) = 2u;                 /* enable */
}

void spi0_set_speed(uint32_t hz)
{
    uint32_t cpsr, scr = 0;
    uint32_t div = (CPU_HZ + hz - 1u) / hz;
    for (cpsr = 2; cpsr <= 254; cpsr += 2) {
        scr = (div + cpsr - 1u) / cpsr;      /* scr + 1 */
        if (scr - 1u <= 255u) break;
    }
    scr -= 1u;
    REG(SPI_CR1) = 0;
    REG(SPI_CR0) = 7u | (scr << 8);
    REG(SPI_CPSR) = cpsr;
    REG(SPI_CR1) = 2u;
}

uint8_t spi0_xfer(uint8_t b)
{
    while (!(REG(SPI_SR) & 2u)) { }    /* TX not full  */
    REG(SPI_DR) = b;
    while (!(REG(SPI_SR) & 4u)) { }    /* RX not empty */
    return (uint8_t)REG(SPI_DR);
}

void spi0_write(const uint8_t *p, uint32_t n)
{
    while (n--) {
        while (!(REG(SPI_SR) & 2u)) { }
        REG(SPI_DR) = *p++;
        if (REG(SPI_SR) & 4u) (void)REG(SPI_DR);
    }
    spi0_flush();
}

void spi0_read(uint8_t *p, uint32_t n)
{
    while (n--) *p++ = spi0_xfer(0xff);
}

void spi0_flush(void)
{
    while (!(REG(SPI_SR) & 1u)) { }    /* TX FIFO empty */
    while (REG(SPI_SR) & (1u << 4)) { }/* not busy      */
    while (REG(SPI_SR) & 4u) (void)REG(SPI_DR);
}
