/* The BIOS call table.  The BIOS puts one of these in flash at a fixed
 * address (0x10000200).  The OS finds it there, so the OS never needs to know
 * how the BIOS is built -- the same idea as old PC BIOS interrupts. */
#ifndef BIOS_API_H
#define BIOS_API_H
#include <stdint.h>

#define BIOS_API_ADDR 0x10000200u
#define BIOS_MAGIC    0x534F4942u      /* "BIOS" */
#define BIOS_VERSION  0x00010000u
#define OS_FLASH_BASE 0x10040000u
#define OS_RAM_BASE   0x20008000u      /* OS owns RAM from here up */
#define RAM_END       0x20082000u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t cpu_hz;
    void     (*putc)(char c);                     /* UART0, raw */
    int      (*getc)(void);                       /* UART0, -1 if none */
    int      (*kbd_getc)(void);                   /* matrix keyboard, -1 if none */
    uint32_t (*millis)(void);
    uint32_t (*micros)(void);
    void     (*delay_ms)(uint32_t ms);
    void     (*led)(int on);
    void     (*lcd_enable)(int on);
    void     (*lcd_fill)(int x, int y, int w, int h, uint16_t rgb565);
    void     (*lcd_draw_row)(int row, const char *text40, int cursor_col,
                             uint16_t fg, uint16_t bg);
    int      (*sd_init)(void);                    /* 0 = ok */
    int      (*sd_read)(uint32_t lba, void *buf512);   /* 0 = ok */
    uint32_t (*sd_blocks)(void);                  /* card size in 512-byte blocks */
    void     (*reboot)(void);
} bios_api_t;

#define BIOS ((const bios_api_t *)BIOS_API_ADDR)
#endif
