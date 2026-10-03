#include "os.h"
#include "rp2350.h"
#include "bios_api.h"
#include "kbd_core.h"
#include <string.h>

extern uint32_t _heap_start, _heap_end;

static fat_fs_t g_fs;
static int g_fs_ok;

uint32_t  os_uptime_ms(void) { return sys_ticks(); }
void      os_reboot(void) { BIOS->reboot(); }
void      os_lcd_enable(int on) { BIOS->lcd_enable(on); if (on) term_clear(); }
fat_fs_t *os_fs(void) { return g_fs_ok ? &g_fs : NULL; }

int os_mount(void)
{
    g_fs_ok = 0;
    int r = BIOS->sd_init();
    if (r) return -100 + r;                       /* < -100: no card */
    r = fat_mount(&g_fs, BIOS->sd_read);
    if (r) return r;
    g_fs_ok = 1;
    return 0;
}

void os_info(void)
{
    heap_stats_t h;
    heap_stats(&h);
    kprintf("%s on BIOS v%u.%u, CPU %u MHz\n", OS_NAME,
            (unsigned)(BIOS->version >> 16), (unsigned)(BIOS->version & 0xffff),
            (unsigned)(BIOS->cpu_hz / 1000000u));
    kprintf("RAM for OS: %u KB, heap free %u KB\n",
            (unsigned)((RAM_END - OS_RAM_BASE) / 1024u), (unsigned)(h.free / 1024u));
    kprintf("SD: %s", g_fs_ok ? "mounted" : "not mounted");
    if (g_fs_ok) kprintf(" (FAT%u)", (unsigned)g_fs.type);
    kprintf("\n");
}

/* console = UART + LCD terminal */
static void con_putc(char c)
{
    if (c == '\n') BIOS->putc('\r');
    BIOS->putc(c);
    term_putc(c);
}

static int con_getc(void)
{
    int c = BIOS->getc();
    if (c < 0) c = BIOS->kbd_getc();
    return c;
}

static void idle_task(void *arg)
{
    (void)arg;
    for (;;) __asm volatile("wfi");
}

static void ui_task(void *arg)
{
    (void)arg;
    for (;;) { term_flush(); task_sleep(33); }
}

static void heartbeat_task(void *arg)
{
    (void)arg;
    for (int on = 0;; on ^= 1) { BIOS->led(on); task_sleep(500); }
}

static void shell_task(void *arg)
{
    (void)arg;
    char line[96];
    int n = 0;
    kprintf("\n%s ready. Type 'help'.\n", OS_NAME);
    for (;;) {
        kprintf("%s> ", shell_cwd());
        n = 0;
        for (;;) {
            int c = con_getc();
            if (c < 0) { task_sleep(5); continue; }
            if (c == '\r' || c == '\n') { kprintf("\n"); break; }
            if (c == 3) { kprintf("^C\n"); n = 0; break; }
            if ((c == 8 || c == 127) && n > 0) { n--; kprintf("\b \b"); continue; }
            if (c >= 32 && c < 127 && n < (int)sizeof line - 1) { line[n++] = (char)c; kprintf("%c", c); }
        }
        line[n] = 0;
        shell_exec(line);
    }
}

/* test builds (QEMU) override these two hooks */
__attribute__((weak)) void os_early_init(void) { }
__attribute__((weak)) void os_extra_tasks(void) { }

int main(void)
{
    os_early_init();
    if (BIOS->magic != BIOS_MAGIC) for (;;) { }   /* started without a BIOS */
    kprintf_set_out(con_putc);
    heap_init(&_heap_start, &_heap_end);
    term_init();
    sched_init();
    shell_init();

    kprintf("%s\n", OS_NAME);
    int r = os_mount();
    if (r == 0) kprintf("SD card mounted (FAT%u)\n", (unsigned)g_fs.type);
    else kprintf("No SD file system (code %d)\n", r);

    task_create("idle",      idle_task,      0, 512);   /* must be task 0 */
    task_create("ui",        ui_task,        0, 2048);
    task_create("heartbeat", heartbeat_task, 0, 1024);
    task_create("shell",     shell_task,     0, 4096);
    os_extra_tasks();
    sched_start();
}
