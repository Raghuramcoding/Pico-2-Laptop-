/* Key matrix scanner: rows GP2..GP9 are outputs, columns are inputs with
 * pull-downs.  Put the diode's stripe (cathode) on the column side. */
#include "bios.h"
#include "kbd_core.h"

static const uint8_t col_pin[KB_COLS] = {10, 11, 12, 13, 14, 15, 26, 27, 28};
#define ROW_PIN0 2u

void bios_kbd_init(void)
{
    for (unsigned r = 0; r < KB_ROWS; r++) gpio_out_init(ROW_PIN0 + r, false);
    for (unsigned c = 0; c < KB_COLS; c++) gpio_in_init(col_pin[c], PAD_PDE);
    kbd_core_init();
}

void bios_kbd_raw(uint16_t raw[KB_ROWS])
{
    for (unsigned r = 0; r < KB_ROWS; r++) {
        gpio_hi(ROW_PIN0 + r);
        delay_us(4);
        uint32_t in = REG(SIO_GPIO_IN);
        gpio_lo(ROW_PIN0 + r);
        uint16_t m = 0;
        for (unsigned c = 0; c < KB_COLS; c++)
            if ((in >> col_pin[c]) & 1u) m |= (uint16_t)(1u << c);
        raw[r] = m;
    }
}

int bios_kbd_getc(void)
{
    static uint32_t last_scan;
    uint32_t now = micros();
    if ((uint32_t)(now - last_scan) >= 5000u) {       /* scan every 5 ms */
        last_scan = now;
        uint16_t raw[KB_ROWS];
        bios_kbd_raw(raw);
        kbd_core_feed(raw, millis());
    }
    return kbd_core_getc();
}
