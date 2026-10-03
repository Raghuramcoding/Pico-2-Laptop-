/* PHASE 0 -- proof of life.
 *   1. Blink 5 times fast on the boot ROM's default clock  -> image + LED work
 *   2. Start the crystal + PLL (150 MHz)                    -> clocks work
 *   3. Blink exactly once per second forever, and print a counter on UART0
 *      (GP0 = TX, GP1 = RX, 115200 8N1).
 * LED patterns:  N slow blinks, pause, repeat = clocks_init() failed (see hal.h)
 *                very fast blink forever       = hard fault              */
#include "hal.h"
#include "kprintf.h"

static void console_putc(char c) { if (c == '\n') uart_putc('\r'); uart_putc(c); }

int main(void)
{
    gpio_config(PIN_LED, FN_SIO, PAD_IE | PAD_SCHMITT);   /* needs IO+PADS out of reset */
    if (!reset_release(RST_IO_BANK0 | RST_PADS_BANK0)) for (;;) { }
    gpio_out_init(PIN_LED, false);

    for (int i = 0; i < 5; i++) {                          /* step 1 */
        gpio_hi(PIN_LED);
        for (volatile uint32_t k = 0; k < 150000; k++) { }
        gpio_lo(PIN_LED);
        for (volatile uint32_t k = 0; k < 150000; k++) { }
    }

    int rc = clocks_init();                                /* step 2 */
    if (rc) fail_blink(rc);
    timer_init();
    uart_init(115200);
    kprintf_set_out(console_putc);
    kprintf("\nPhase 0 OK: clocks running at %u MHz\n", CPU_HZ / 1000000u);

    uint32_t next = micros();                              /* step 3 */
    for (unsigned n = 0;; n++) {
        gpio_hi(PIN_LED);
        next += 500000u;
        while ((int32_t)(micros() - next) < 0) { }
        gpio_lo(PIN_LED);
        next += 500000u;
        while ((int32_t)(micros() - next) < 0) { }
        kprintf("tick %u\n", n);
    }
}
