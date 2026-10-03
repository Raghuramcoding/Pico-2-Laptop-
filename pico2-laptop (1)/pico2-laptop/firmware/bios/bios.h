#ifndef BIOS_H
#define BIOS_H
#include <stdint.h>
#include "hal.h"
#include "bios_api.h"

#define PIN_SD_CS   17u
#define PIN_LCD_CS  20u
#define PIN_LCD_DC  21u
#define PIN_LCD_RST 22u

/* lcd.c */
void lcd_init(void);
void lcd_set_enabled(int on);
void lcd_fill(int x, int y, int w, int h, uint16_t rgb565);
void lcd_draw_row(int row, const char *text40, int cursor_col, uint16_t fg, uint16_t bg);
/* sd.c */
int      sd_init(void);
int      sd_read_block(uint32_t lba, void *buf512);
uint32_t sd_block_count(void);
/* kbd.c */
void bios_kbd_init(void);
int  bios_kbd_getc(void);
void bios_kbd_raw(uint16_t raw[8]);
/* api.c */
extern const bios_api_t bios_api;
void bios_reboot(void);

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | ((b) >> 3)))
#endif
