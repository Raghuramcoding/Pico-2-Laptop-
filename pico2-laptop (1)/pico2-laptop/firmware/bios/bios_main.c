/* PHASE 1 -- the BIOS.
 * Power-on self test, hardware bring-up, a small monitor on the serial port,
 * and the hand-off to the OS stored at 0x10040000. */
#include <string.h>
#include "bios.h"
#include "kprintf.h"
#include "kbd_core.h"

#define BIOS_NAME "Pico2 Laptop BIOS v0.1"
#define COL_FG RGB565(0x80, 0xff, 0x80)
#define COL_BG RGB565(0, 0, 0)

static void con_putc(char c) { if (c == '\n') uart_putc('\r'); uart_putc(c); }

/* ---- status text: goes to UART and to the LCD ---- */
static int lrow;
static void say_str(const char *s)
{
    kprintf("%s\n", s);
    lcd_draw_row(lrow, s, -1, COL_FG, COL_BG);
    if (++lrow >= 30) lrow = 0;
}
#define SAY(...) do { char _b[48]; ksnprintf(_b, sizeof _b, __VA_ARGS__); say_str(_b); } while (0)

/* returns 0 if ok, else the first bad address */
static uint32_t ram_test(uint32_t lo, uint32_t hi)
{
    for (uint32_t a = lo; a < hi; a += 4) REG(a) = a ^ 0xa5a5a5a5u;
    for (uint32_t a = lo; a < hi; a += 4) if (REG(a) != (a ^ 0xa5a5a5a5u)) return a;
    for (uint32_t a = lo; a < hi; a += 4) REG(a) = ~a;
    for (uint32_t a = lo; a < hi; a += 4) if (REG(a) != ~a) return a;
    return 0;
}

static int os_valid(void)
{
    const uint32_t *vt = (const uint32_t *)OS_FLASH_BASE;
    uint32_t sp = vt[0], pc = vt[1];
    return sp > OS_RAM_BASE && sp <= RAM_END && (sp & 7u) == 0 &&
           (pc & 1u) && pc > OS_FLASH_BASE && pc < 0x10400000u;
}

static void boot_os(void)
{
    const uint32_t *vt = (const uint32_t *)OS_FLASH_BASE;
    uint32_t sp = vt[0], pc = vt[1];
    kprintf("Jumping to OS at %p (sp=%p)\n", (void *)pc, (void *)sp);
    uart_flush();
    __asm volatile("msr msp, %0\n\t"
                   "bx %1\n\t" :: "r"(sp), "r"(pc));
    __builtin_unreachable();
}

/* ---- serial monitor ---- */
static int readline(char *buf, int max)
{
    int n = 0;
    for (;;) {
        int c = uart_getc();
        if (c < 0) continue;
        if (c == '\r' || c == '\n') { kprintf("\n"); buf[n] = 0; return n; }
        if ((c == 8 || c == 127) && n > 0) { n--; kprintf("\b \b"); continue; }
        if (c >= 32 && c < 127 && n < max - 1) { buf[n++] = (char)c; uart_putc((char)c); }
    }
}

static int parse_hex(const char *s, uint32_t *out)
{
    uint32_t v = 0; int any = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else return 0;
        v = (v << 4) | (uint32_t)d; any = 1;
    }
    *out = v;
    return any;
}

static void dump(uint32_t addr, uint32_t len)
{
    for (uint32_t i = 0; i < len; i += 16) {
        kprintf("%08x: ", addr + i);
        for (uint32_t j = 0; j < 16 && i + j < len; j++)
            kprintf("%02x ", *(volatile uint8_t *)(addr + i + j));
        kprintf("\n");
    }
}

static void cmd_help(void)
{
    kprintf("help            this list\n"
            "info            hardware summary\n"
            "boot            start the OS\n"
            "md ADDR [N]     dump N bytes at hex ADDR (bad ADDR = hard fault!)\n"
            "sd              (re)initialise the SD card\n"
            "sdr LBA         read one SD sector, show first 64 bytes\n"
            "lcd             colour bars\n"
            "kbd             show key codes (press ESC to stop)\n"
            "reboot          restart\n");
}

static void cmd_info(void)
{
    kprintf("%s\nCPU    %u MHz\nOS     %s\nSD     %u blocks\n",
            BIOS_NAME, CPU_HZ / 1000000u, os_valid() ? "present" : "not found",
            sd_block_count());
}

static void cmd_lcd(void)
{
    static const uint16_t cols[8] = {
        RGB565(255,255,255), RGB565(255,255,0), RGB565(0,255,255), RGB565(0,255,0),
        RGB565(255,0,255), RGB565(255,0,0), RGB565(0,0,255), RGB565(0,0,0) };
    for (int i = 0; i < 8; i++) lcd_fill(i * 40, 0, 40, 240, cols[i]);
    kprintf("8 colour bars drawn. press Enter to clear.\n");
    while (uart_getc() != '\r') { }
    lcd_fill(0, 0, 320, 240, COL_BG);
    lrow = 0;
}

static void cmd_kbd(void)
{
    kprintf("Press keys on the matrix keyboard; ESC quits.\n");
    for (;;) {
        int c = bios_kbd_getc();
        if (c < 0) continue;
        kprintf("code %d (0x%02x) '%c'\n", c, c, (c >= 32 && c < 127) ? c : '.');
        if (c == K_ESC) break;
    }
}

static void monitor(void)
{
    char line[64];
    kprintf("\nBIOS monitor. Type 'help'.\n");
    for (;;) {
        kprintf("bios> ");
        if (readline(line, sizeof line) == 0) continue;
        char *arg1 = line;
        while (*arg1 && *arg1 != ' ') arg1++;
        char *arg2 = 0;
        if (*arg1) { *arg1++ = 0; while (*arg1 == ' ') arg1++; }
        for (char *p = arg1; *p; p++) if (*p == ' ') { *p = 0; arg2 = p + 1; break; }

        if (!strcmp(line, "help")) cmd_help();
        else if (!strcmp(line, "info")) cmd_info();
        else if (!strcmp(line, "boot")) { if (os_valid()) boot_os(); else kprintf("No OS at %p\n", (void *)OS_FLASH_BASE); }
        else if (!strcmp(line, "md")) {
            uint32_t a, n = 64;
            if (!parse_hex(arg1, &a)) { kprintf("usage: md ADDR [N]\n"); continue; }
            if (arg2) parse_hex(arg2, &n);
            if (n > 1024) n = 1024;
            dump(a, n);
        }
        else if (!strcmp(line, "sd")) {
            int rc = sd_init();
            kprintf("sd_init -> %d (%s), %u blocks\n", rc, rc == 0 ? "ok" : "failed", sd_block_count());
        }
        else if (!strcmp(line, "sdr")) {
            uint32_t lba;
            static uint8_t sect[512];
            if (!parse_hex(arg1, &lba)) { kprintf("usage: sdr HEXLBA\n"); continue; }
            int rc = sd_read_block(lba, sect);
            if (rc) kprintf("read failed (%d)\n", rc); else dump((uint32_t)(uintptr_t)sect, 64);
        }
        else if (!strcmp(line, "lcd")) cmd_lcd();
        else if (!strcmp(line, "kbd")) cmd_kbd();
        else if (!strcmp(line, "reboot")) bios_reboot();
        else kprintf("unknown command '%s' (try help)\n", line);
    }
}

int main(void)
{
    REG(RESETS_BASE) &= ~(RST_IO_BANK0 | RST_PADS_BANK0);
    for (volatile uint32_t k = 0; k < 20000; k++) { }
    gpio_out_init(PIN_LED, true);                       /* LED on = BIOS running */

    int rc = clocks_init();
    if (rc) fail_blink(rc);
    timer_init();
    uart_init(115200);
    kprintf_set_out(con_putc);
    kprintf("\n\n%s\n", BIOS_NAME);

    spi0_init();
    lcd_init();
    bios_kbd_init();
    lcd_fill(0, 0, 320, 240, COL_BG);
    say_str(BIOS_NAME);
    SAY("CPU %u MHz", CPU_HZ / 1000000u);

    uint32_t bad = ram_test(OS_RAM_BASE, RAM_END);
    if (bad) { SAY("RAM FAIL at %p", (void *)bad); fail_blink(5); }
    SAY("RAM %u KB ok", (RAM_END - OS_RAM_BASE) / 1024u);

    rc = sd_init();
    if (rc == 0) SAY("SD card: %u MB", sd_block_count() / 2048u);
    else SAY("SD card: none (%d)", rc);
    SAY("OS image: %s", os_valid() ? "found" : "NOT FOUND");

    if (os_valid()) {
        kprintf("Press any key for the BIOS monitor...\n");
        for (int s = 3; s > 0; s--) {
            SAY("Booting OS in %d s (key=BIOS)", s);
            uint32_t t0 = millis();
            while ((uint32_t)(millis() - t0) < 1000) {
                if (uart_getc() >= 0 || bios_kbd_getc() >= 0) { monitor(); }
            }
        }
        boot_os();
    }
    monitor();
    return 0;
}
