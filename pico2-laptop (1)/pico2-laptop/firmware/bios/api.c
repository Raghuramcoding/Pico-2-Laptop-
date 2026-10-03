#include "bios.h"

static void api_led(int on) { if (on) gpio_hi(PIN_LED); else gpio_lo(PIN_LED); }

void bios_reboot(void)
{
    uart_flush();
    REG(SCB_AIRCR) = 0x05FA0004u;            /* SYSRESETREQ */
    for (volatile int i = 0; i < 100000; i++) { }
    REG(WATCHDOG_BASE) = 1u << 31;           /* fallback: watchdog trigger */
    for (;;) { }
}

__attribute__((section(".bios_api"), used))
const bios_api_t bios_api = {
    .magic = BIOS_MAGIC,
    .version = BIOS_VERSION,
    .cpu_hz = CPU_HZ,
    .putc = uart_putc,
    .getc = uart_getc,
    .kbd_getc = bios_kbd_getc,
    .millis = millis,
    .micros = micros,
    .delay_ms = delay_ms,
    .led = api_led,
    .lcd_enable = lcd_set_enabled,
    .lcd_fill = lcd_fill,
    .lcd_draw_row = lcd_draw_row,
    .sd_init = sd_init,
    .sd_read = sd_read_block,
    .sd_blocks = sd_block_count,
    .reboot = bios_reboot,
};
