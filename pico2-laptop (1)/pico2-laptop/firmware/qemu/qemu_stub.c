/* TEST ONLY: stands in for the BIOS when the OS runs inside QEMU (mps2-an505,
 * Cortex-M33).  Uses the board's CMSDK UART instead of the RP2350 one. */
#include "os.h"
#include "bios_api.h"

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define QUART 0x40200000u
static void q_putc(char c)
{
    while (REG32(QUART + 4) & 1u) { }
    REG32(QUART) = (uint32_t)(uint8_t)c;
}
static int q_getc(void)
{
    if (!(REG32(QUART + 4) & 2u)) return -1;
    return (int)(REG32(QUART) & 0xffu);
}
static int q_kbd(void) { return -1; }
static uint32_t q_ms(void) { return sys_ticks(); }
static void q_delay(uint32_t ms) { task_sleep(ms); }
static void q_led(int on) { (void)on; }
static void q_lcd_en(int on) { (void)on; }
static void q_fill(int x, int y, int w, int h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
/* "LCD" = second serial port.  Each redrawn row is sent as: R<row>,<cursor>,<40 chars>\n
 * tools/vm_screen.py turns that stream back into a picture of the screen. */
#define QUART1 0x40201000u
static void q1_putc(char c)
{
    while (REG32(QUART1 + 4) & 1u) { }
    REG32(QUART1) = (uint32_t)(uint8_t)c;
}
static void q_row(int r, const char *t, int cc, uint16_t f, uint16_t b)
{
    (void)f; (void)b;
    char hdr[12];
    ksnprintf(hdr, sizeof hdr, "R%d,%d,", r, cc);
    for (const char *p = hdr; *p; p++) q1_putc(*p);
    for (int i = 0; i < 40; i++) q1_putc(t[i] ? t[i] : ' ');
    q1_putc('\n');
}
/* "SD card" = a disk image the launcher loads into RAM at VM_DISK_ADDR */
#define VM_DISK_ADDR 0x80000000u
#define VM_DISK_MAX  (16u * 1024u * 1024u)
static int q_sd_init(void)
{
    const uint8_t *d = (const uint8_t *)(uintptr_t)VM_DISK_ADDR;
    return (d[510] == 0x55 && d[511] == 0xAA) ? 0 : -2;      /* no disk loaded */
}
static int q_sd_read(uint32_t l, void *b)
{
    if ((l + 1u) * 512u > VM_DISK_MAX) return -1;
    const uint8_t *src = (const uint8_t *)(uintptr_t)(VM_DISK_ADDR + l * 512u);
    uint8_t *dst = b;
    for (int i = 0; i < 512; i++) dst[i] = src[i];
    return 0;
}
static uint32_t q_blocks(void) { return VM_DISK_MAX / 512u; }
static void q_reboot(void) { kprintf("(reboot requested)\n"); for (;;) { } }

__attribute__((section(".bios_api"), used))
const bios_api_t qemu_bios = {
    .magic = BIOS_MAGIC, .version = BIOS_VERSION, .cpu_hz = 20000000u,
    .putc = q_putc, .getc = q_getc, .kbd_getc = q_kbd, .millis = q_ms, .micros = q_ms,
    .delay_ms = q_delay, .led = q_led, .lcd_enable = q_lcd_en, .lcd_fill = q_fill,
    .lcd_draw_row = q_row, .sd_init = q_sd_init, .sd_read = q_sd_read,
    .sd_blocks = q_blocks, .reboot = q_reboot,
};

void os_early_init(void)
{
    REG32(QUART + 0x10) = 16;       /* baud divider */
    REG32(QUART + 0x08) = 3;        /* TX + RX enable */
    REG32(QUART1 + 0x10) = 16;
    REG32(QUART1 + 0x08) = 1;       /* TX only */
}

#ifdef VM_DEMO_TASKS
static volatile uint32_t spin_count;
static void spin_task(void *a) { (void)a; for (;;) spin_count++; }    /* never yields */
static void count_task(void *a)
{
    (void)a;
    for (int i = 1; i <= 5; i++) { task_sleep(100); kprintf("[counter] tick %d (spinner at %u)\n", i, (unsigned)(spin_count > 0)); }
}
void os_extra_tasks(void)
{
    task_create("spin", spin_task, 0, 512);
    task_create("counter", count_task, 0, 1024);
}
#else
void os_extra_tasks(void) { }
#endif
