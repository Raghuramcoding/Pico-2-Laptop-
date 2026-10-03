/* Host-side tests: run on a PC, no hardware needed.
 *   test_all selftest          -> printf, heap, paths, keyboard, shell
 *   test_all fat IMAGE MANIFEST [OFFSET_BYTES] -> FAT driver against a real image */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "os.h"
#include "kbd_core.h"

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* ---- capture kprintf output ---- */
static char cap[8192]; static size_t capn;
static void cap_putc(char c) { if (capn < sizeof cap - 1) cap[capn++] = c; cap[capn] = 0; }
static void cap_reset(void) { capn = 0; cap[0] = 0; }

/* ---- stubs for services the shell needs ---- */
static FILE *img; static long img_off; static fat_fs_t g_fs; static int g_mounted;
static int rd_img(uint32_t lba, void *buf)
{
    if (fseek(img, img_off + (long)lba * 512, SEEK_SET)) return -1;
    return fread(buf, 1, 512, img) == 512 ? 0 : -1;
}
uint32_t os_uptime_ms(void) { return 3723456; }
void os_reboot(void) { printf("(reboot)\n"); }
void os_lcd_enable(int on) { (void)on; }
void os_info(void) { kprintf("info\n"); }
fat_fs_t *os_fs(void) { return g_mounted ? &g_fs : NULL; }
int os_mount(void) { return 0; }
void term_clear(void) { kprintf("<clear>"); }
void sched_ps(void) { kprintf("<ps>"); }

static uint32_t fnv(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    while (n--) { h ^= *p++; h *= 16777619u; }
    return h;
}

static void test_printf(void)
{
    char b[64];
    ksnprintf(b, sizeof b, "%d|%5d|%-5d|%05d|%u|%x|%X|%c|%s|%%", -42, 42, 42, 42, 4000000000u, 255, 255, 'z', "hi");
    CHECK(!strcmp(b, "-42|   42|42   |00042|4000000000|ff|FF|z|hi|%"));
    ksnprintf(b, sizeof b, "%08x %-4s| %4s", 0xBEEFu, "ab", "cd");
    CHECK(!strcmp(b, "0000beef ab  |   cd"));
    ksnprintf(b, sizeof b, "%d", -2147483647 - 1);
    CHECK(!strcmp(b, "-2147483648"));
    ksnprintf(b, 5, "abcdefgh");
    CHECK(!strcmp(b, "abcd"));
}

static void test_heap(void)
{
    static uint8_t arena[64 * 1024] __attribute__((aligned(8)));
    heap_init(arena, arena + sizeof arena);
    heap_stats_t s0, s;
    heap_stats(&s0);
    CHECK(s0.blocks_free == 1 && s0.blocks_used == 0);
    void *p[200]; size_t sz[200];
    srand(1);
    for (int i = 0; i < 200; i++) {
        sz[i] = (size_t)(rand() % 300) + 1;
        p[i] = kmalloc(sz[i]);
        CHECK(p[i] != NULL);
        CHECK(((uintptr_t)p[i] & 7) == 0);
        memset(p[i], i & 0xff, sz[i]);
    }
    for (int i = 0; i < 200; i++)                   /* nothing overwritten by neighbours */
        for (size_t k = 0; k < sz[i]; k++) if (((uint8_t *)p[i])[k] != (i & 0xff)) { CHECK(0); break; }
    for (int i = 0; i < 200; i += 2) kfree(p[i]);   /* free every other one */
    for (int i = 0; i < 100; i++) { void *q = kmalloc(8 + (size_t)(i % 40)); CHECK(q != NULL); kfree(q); }
    for (int i = 1; i < 200; i += 2) kfree(p[i]);
    heap_stats(&s);
    CHECK(s.blocks_used == 0 && s.blocks_free == 1 && s.free == s0.free);   /* fully merged */
    CHECK(kmalloc(1 << 20) == NULL);
}

static void test_paths(void)
{
    char o[64];
    CHECK(path_resolve("/", "a", o, sizeof o) == 0 && !strcmp(o, "/a"));
    CHECK(path_resolve("/a/b", "../c", o, sizeof o) == 0 && !strcmp(o, "/a/c"));
    CHECK(path_resolve("/a", "../../..", o, sizeof o) == 0 && !strcmp(o, "/"));
    CHECK(path_resolve("/a", "/x//y/./z/", o, sizeof o) == 0 && !strcmp(o, "/x/y/z"));
    CHECK(path_resolve("/", ".", o, sizeof o) == 0 && !strcmp(o, "/"));
    CHECK(path_resolve("/", "averyveryverylongname/another", o, 10) < 0);
}

static void scan(const uint16_t raw[KB_ROWS], int times, uint32_t *t)
{
    for (int i = 0; i < times; i++) { kbd_core_feed(raw, *t); *t += 5; }
}

static void test_kbd(void)
{
    uint16_t none[KB_ROWS] = {0}, a[KB_ROWS] = {0}, shift_a[KB_ROWS] = {0}, ctrl_c[KB_ROWS] = {0}, caps[KB_ROWS] = {0}, glitch[KB_ROWS] = {0};
    uint32_t t = 0;
    a[4] = 1u << 1;                                     /* 'a' is row4 col1 */
    shift_a[4] = 1u << 1; shift_a[6] = 1u << 0;         /* + LSHIFT row6 col0 */
    ctrl_c[0] = 1u << 7; ctrl_c[6] = 1u << 3;           /* CTRL row0 col7, 'c' row6 col3 */
    caps[4] = 1u << 0;                                  /* CAPS row4 col0 */
    glitch[2] = 1u << 1;                                /* 'q' for one scan only */
    kbd_core_init();

    scan(none, 3, &t);
    CHECK(kbd_core_getc() == -1);
    scan(glitch, 1, &t); scan(none, 3, &t);             /* single-scan glitch ignored */
    CHECK(kbd_core_getc() == -1);
    scan(a, 3, &t); scan(none, 3, &t);
    CHECK(kbd_core_getc() == 'a'); CHECK(kbd_core_getc() == -1);
    scan(shift_a, 3, &t); scan(none, 3, &t);
    CHECK(kbd_core_getc() == 'A');
    scan(ctrl_c, 3, &t); scan(none, 3, &t);
    CHECK(kbd_core_getc() == 3);
    scan(caps, 3, &t); scan(none, 3, &t);               /* caps lock on */
    scan(a, 3, &t); scan(none, 3, &t);
    CHECK(kbd_core_getc() == 'A');
    scan(caps, 3, &t); scan(none, 3, &t);               /* off again */
    scan(a, 3, &t); scan(none, 3, &t);
    CHECK(kbd_core_getc() == 'a');
    /* auto-repeat: hold 'a' for 1 s -> first press + roughly (1000-500)/40 repeats */
    scan(a, 200, &t); scan(none, 3, &t);
    int n = 0; while (kbd_core_getc() == 'a') n++;
    CHECK(n >= 10 && n <= 16);
}

static void run(const char *cmd)
{
    char line[128];
    strcpy(line, cmd);
    cap_reset();
    shell_exec(line);
}

static void test_shell_basic(void)
{
    run("echo hello   world");  CHECK(!strcmp(cap, "hello world\n"));
    run("echo \"two words\" x"); CHECK(!strcmp(cap, "two words x\n"));
    run("uptime");              CHECK(!strcmp(cap, "up 1:02:03.456\n"));
    run("nosuch");              CHECK(strstr(cap, "unknown command 'nosuch'") != NULL);
    run("ls");                  CHECK(strstr(cap, "No file system") != NULL);
    run("mem");                 CHECK(strstr(cap, "heap used") != NULL);
    run("peek zz");             CHECK(strstr(cap, "bad address") != NULL);
    /* peek takes 32-bit addresses (like the real chip): put test data below 4 GB */
    uint32_t *words = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    CHECK(words != MAP_FAILED);
    words[0] = 0xdeadbeef; words[1] = 0x12345678;
    char cmdline[64];
    snprintf(cmdline, sizeof cmdline, "peek %lx 2", (unsigned long)(uintptr_t)words);
    run(cmdline);               CHECK(strstr(cap, "deadbeef 12345678") != NULL);
    run("");                    CHECK(cap[0] == 0);
    run("help");                CHECK(strstr(cap, "cat") && strstr(cap, "reboot"));
}

static int test_fat(const char *image, const char *manifest, long off)
{
    img = fopen(image, "rb"); img_off = off;
    if (!img) { printf("cannot open %s\n", image); return 1; }
    CHECK(fat_mount(&g_fs, rd_img) == FAT_OK);
    g_mounted = 1;
    FILE *m = fopen(manifest, "r");
    char kind[4], path[128]; unsigned size, hash;
    int files = 0;
    while (m && fscanf(m, "%3s %127s %u %x", kind, path, &size, &hash) == 4) {
        if (kind[0] == 'F') {
            fat_file_t f;
            CHECK(fat_open(&g_fs, path, &f) == FAT_OK);
            CHECK(f.size == size && !f.is_dir);
            uint8_t *buf = malloc(size + 1);
            uint32_t got = 0; int chunk[] = { 1, 100, 511, 512, 513, 4000 }, ci = 0;
            while (got < size) {
                int want = chunk[ci++ % 6];
                int r = fat_read(&g_fs, &f, buf + got, (uint32_t)want);
                CHECK(r > 0);
                if (r <= 0) break;
                got += (uint32_t)r;
            }
            CHECK(got == size && fnv(buf, size) == hash);
            CHECK(fat_read(&g_fs, &f, buf, 10) == 0);        /* at EOF */
            free(buf); files++;
        } else if (kind[0] == 'D') {
            fat_file_t f;
            CHECK(fat_open(&g_fs, path, &f) == FAT_OK && f.is_dir);
        }
    }
    if (m) fclose(m);
    CHECK(files >= 3);

    fat_file_t f;
    CHECK(fat_open(&g_fs, "/NOPE.TXT", &f) == FAT_ENOENT);
    CHECK(fat_open(&g_fs, "/readme.txt/x", &f) == FAT_ENOTDIR);
    CHECK(fat_open(&g_fs, "/sub/../SMALL.TXT", &f) == FAT_OK);   /* ".." inside a subdir */

    run("ls");        CHECK((strstr(cap, "SMALL.TXT") || strstr(cap, "small.txt")) && (strstr(cap, "SUB") || strstr(cap, "sub")) && strstr(cap, "<DIR>"));
    run("cd sub");    CHECK(!strcmp(shell_cwd(), "/SUB") || !strcmp(shell_cwd(), "/sub"));
    run("ls");        CHECK(strstr(cap, "INNER.TXT") != NULL);
    run("cat inner.txt"); CHECK(strstr(cap, "inner file") != NULL);
    run("cd ..");     CHECK(!strcmp(shell_cwd(), "/"));
    run("cat small.txt"); CHECK(strstr(cap, "hello from the sd card") != NULL);
    run("cat nothing.txt"); CHECK(strstr(cap, "no such file") != NULL);
    run("cd small.txt"); CHECK(strstr(cap, "not a directory") != NULL);
    fclose(img);
    return 0;
}

int main(int argc, char **argv)
{
    kprintf_set_out(cap_putc);
    if (argc >= 4 && !strcmp(argv[1], "fat")) {
        test_fat(argv[2], argv[3], argc > 4 ? atol(argv[4]) : 0);
        printf("fat %s: %d checks, %d failures\n", argv[2], checks, failures);
        return failures != 0;
    }
    test_printf(); test_heap(); test_paths(); test_kbd(); test_shell_basic();
    printf("selftest: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
