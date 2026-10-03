/* ILI9341 320x240 SPI display, landscape, text mode (40 x 30 cells of 8x8). */
#include "bios.h"
#include "irq.h"
#include "font8x8.h"

#define LCD_HZ 37500000u
static int lcd_on = 1;
static uint8_t rowbuf[320 * 8 * 2];

static void sel(void)   { spi0_set_speed(LCD_HZ); gpio_lo(PIN_LCD_CS); }
static void unsel(void) { spi0_flush(); gpio_hi(PIN_LCD_CS); }

static void cmd(uint8_t c)
{
    sel(); gpio_lo(PIN_LCD_DC); spi0_write(&c, 1); unsel();
}
static void data(const uint8_t *d, uint32_t n)
{
    sel(); gpio_hi(PIN_LCD_DC); spi0_write(d, n); unsel();
}
static void cmd_d(uint8_t c, const uint8_t *d, uint32_t n)
{
    cmd(c);
    if (n) data(d, n);
}

static void window(int x0, int y0, int x1, int y1)
{
    uint8_t a[4];
    a[0] = (uint8_t)(x0 >> 8); a[1] = (uint8_t)x0; a[2] = (uint8_t)(x1 >> 8); a[3] = (uint8_t)x1;
    cmd_d(0x2a, a, 4);
    a[0] = (uint8_t)(y0 >> 8); a[1] = (uint8_t)y0; a[2] = (uint8_t)(y1 >> 8); a[3] = (uint8_t)y1;
    cmd_d(0x2b, a, 4);
    cmd(0x2c);
}

void lcd_set_enabled(int on) { lcd_on = on; }

void lcd_init(void)
{
    gpio_out_init(PIN_LCD_CS, true);
    gpio_out_init(PIN_LCD_DC, true);
    gpio_out_init(PIN_LCD_RST, true);
    gpio_out_init(PIN_SD_CS, true);
    delay_ms(5);
    gpio_lo(PIN_LCD_RST); delay_ms(20);
    gpio_hi(PIN_LCD_RST); delay_ms(150);

    static const uint8_t i_ef[] = {0x03, 0x80, 0x02};
    static const uint8_t i_cf[] = {0x00, 0xc1, 0x30};
    static const uint8_t i_ed[] = {0x64, 0x03, 0x12, 0x81};
    static const uint8_t i_e8[] = {0x85, 0x00, 0x78};
    static const uint8_t i_cb[] = {0x39, 0x2c, 0x00, 0x34, 0x02};
    static const uint8_t i_f7[] = {0x20};
    static const uint8_t i_ea[] = {0x00, 0x00};
    static const uint8_t i_c0[] = {0x23};
    static const uint8_t i_c1[] = {0x10};
    static const uint8_t i_c5[] = {0x3e, 0x28};
    static const uint8_t i_c7[] = {0x86};
    static const uint8_t i_36[] = {0x28};         /* MV | BGR : landscape. try 0xE8 if upside down */
    static const uint8_t i_3a[] = {0x55};         /* 16 bits per pixel */
    static const uint8_t i_b1[] = {0x00, 0x18};
    static const uint8_t i_b6[] = {0x08, 0x82, 0x27};
    static const uint8_t i_f2[] = {0x00};
    static const uint8_t i_26[] = {0x01};
    static const uint8_t i_e0[] = {0x0f,0x31,0x2b,0x0c,0x0e,0x08,0x4e,0xf1,0x37,0x07,0x10,0x03,0x0e,0x09,0x00};
    static const uint8_t i_e1[] = {0x00,0x0e,0x14,0x03,0x11,0x07,0x31,0xc1,0x48,0x08,0x0f,0x0c,0x31,0x36,0x0f};

    cmd(0x01); delay_ms(150);                      /* software reset */
    cmd_d(0xef, i_ef, 3);  cmd_d(0xcf, i_cf, 3);  cmd_d(0xed, i_ed, 4);
    cmd_d(0xe8, i_e8, 3);  cmd_d(0xcb, i_cb, 5);  cmd_d(0xf7, i_f7, 1);
    cmd_d(0xea, i_ea, 2);  cmd_d(0xc0, i_c0, 1);  cmd_d(0xc1, i_c1, 1);
    cmd_d(0xc5, i_c5, 2);  cmd_d(0xc7, i_c7, 1);  cmd_d(0x36, i_36, 1);
    cmd_d(0x3a, i_3a, 1);  cmd_d(0xb1, i_b1, 2);  cmd_d(0xb6, i_b6, 3);
    cmd_d(0xf2, i_f2, 1);  cmd_d(0x26, i_26, 1);
    cmd_d(0xe0, i_e0, 15); cmd_d(0xe1, i_e1, 15);
    cmd(0x11); delay_ms(130);                      /* sleep out */
    cmd(0x29); delay_ms(20);                       /* display on */
    lcd_fill(0, 0, 320, 240, 0x0000);
}

void lcd_fill(int x, int y, int w, int h, uint16_t rgb565)
{
    if (!lcd_on || w <= 0 || h <= 0) return;
    uint32_t irq = irq_save();
    uint8_t chunk[256];
    for (int i = 0; i < 256; i += 2) { chunk[i] = (uint8_t)(rgb565 >> 8); chunk[i + 1] = (uint8_t)rgb565; }
    window(x, y, x + w - 1, y + h - 1);
    sel(); gpio_hi(PIN_LCD_DC);
    uint32_t bytes = (uint32_t)w * (uint32_t)h * 2u;
    while (bytes) {
        uint32_t n = bytes > 256u ? 256u : bytes;
        spi0_write(chunk, n);
        bytes -= n;
    }
    unsel();
    irq_restore(irq);
}

void lcd_draw_row(int row, const char *text, int cursor_col, uint16_t fg, uint16_t bg)
{
    if (!lcd_on || row < 0 || row >= 30) return;
    uint8_t fh = (uint8_t)(fg >> 8), fl = (uint8_t)fg, bh = (uint8_t)(bg >> 8), bl = (uint8_t)bg;
    for (int col = 0; col < 40; col++) {
        unsigned char ch = text[col] ? (unsigned char)text[col] : ' ';
        if (ch < 32 || ch > 126) ch = '?';
        const uint8_t *g = font8x8[ch - 32];
        int inv = (col == cursor_col);
        for (int y = 0; y < 8; y++) {
            uint8_t bits = g[y];
            uint8_t *o = &rowbuf[(y * 320 + col * 8) * 2];
            for (int px = 0; px < 8; px++) {
                int on = ((bits >> px) & 1) ^ inv;
                *o++ = on ? fh : bh;
                *o++ = on ? fl : bl;
            }
        }
    }
    uint32_t irq = irq_save();
    window(0, row * 8, 319, row * 8 + 7);
    sel(); gpio_hi(PIN_LCD_DC);
    spi0_write(rowbuf, sizeof rowbuf);
    unsel();
    irq_restore(irq);
}
