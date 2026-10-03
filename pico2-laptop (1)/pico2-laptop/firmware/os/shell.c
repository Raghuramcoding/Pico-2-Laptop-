/* A small command shell.  shell_exec() runs one line; it only talks to the
 * rest of the OS through os.h so it can be unit-tested on a PC. */
#include "os.h"
#include <string.h>

#define MAX_ARGS 8
static char cwd[128] = "/";

const char *shell_cwd(void) { return cwd; }
void shell_init(void) { strcpy(cwd, "/"); }

static fat_fs_t *need_fs(void)
{
    fat_fs_t *fs = os_fs();
    if (!fs) kprintf("No file system. Insert a FAT32 SD card and run: mount\n");
    return fs;
}

static void ls_cb(const char *name, uint32_t size, bool is_dir, void *ctx)
{
    (void)ctx;
    if (is_dir) kprintf("%-14s <DIR>\n", name);
    else kprintf("%-14s %u\n", name, (unsigned)size);
}

static void c_help(int argc, char **argv);
static void c_echo(int argc, char **argv) { (void)argc; for (int i = 1; i < argc; i++) kprintf("%s%s", i > 1 ? " " : "", argv[i]); kprintf("\n"); }
static void c_clear(int argc, char **argv) { (void)argc; (void)argv; term_clear(); }
static void c_pwd(int argc, char **argv) { (void)argc; (void)argv; kprintf("%s\n", cwd); }

static void c_ls(int argc, char **argv)
{
    fat_fs_t *fs = need_fs();
    if (!fs) return;
    char path[128];
    if (path_resolve(cwd, argc > 1 ? argv[1] : ".", path, sizeof path) < 0) { kprintf("path too long\n"); return; }
    int r = fat_list(fs, path, ls_cb, 0);
    if (r < 0) kprintf("ls: %s: %s\n", path, fat_strerror(r));
}

static void c_cd(int argc, char **argv)
{
    fat_fs_t *fs = need_fs();
    if (!fs) return;
    char path[128];
    if (path_resolve(cwd, argc > 1 ? argv[1] : "/", path, sizeof path) < 0) { kprintf("path too long\n"); return; }
    fat_file_t f;
    int r = fat_open(fs, path, &f);
    if (r == 0 && !f.is_dir) r = FAT_ENOTDIR;
    if (r < 0) { kprintf("cd: %s: %s\n", path, fat_strerror(r)); return; }
    strcpy(cwd, path);
}

static void c_cat(int argc, char **argv)
{
    if (argc < 2) { kprintf("usage: cat FILE\n"); return; }
    fat_fs_t *fs = need_fs();
    if (!fs) return;
    char path[128];
    if (path_resolve(cwd, argv[1], path, sizeof path) < 0) { kprintf("path too long\n"); return; }
    fat_file_t f;
    int r = fat_open(fs, path, &f);
    if (r < 0) { kprintf("cat: %s: %s\n", path, fat_strerror(r)); return; }
    uint8_t buf[128];
    uint32_t total = 0;
    while (total < 16384) {                           /* safety limit */
        r = fat_read(fs, &f, buf, sizeof buf);
        if (r < 0) { kprintf("\ncat: %s\n", fat_strerror(r)); return; }
        if (r == 0) break;
        for (int i = 0; i < r; i++) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127)) kprintf("%c", c);
            else kprintf(".");
        }
        total += (uint32_t)r;
    }
    kprintf("\n");
}

static void c_mount(int argc, char **argv)
{
    (void)argc; (void)argv;
    int r = os_mount();
    if (r == 0) kprintf("SD card mounted.\n");
    else kprintf("mount failed: %s\n", r > -100 ? fat_strerror(r) : "no SD card");
    strcpy(cwd, "/");
}

static void c_mem(int argc, char **argv)
{
    (void)argc; (void)argv;
    heap_stats_t s;
    heap_stats(&s);
    kprintf("heap used %u B in %u blocks, free %u B (largest %u B)\n",
            (unsigned)s.used, (unsigned)s.blocks_used, (unsigned)s.free, (unsigned)s.largest_free);
}

static void c_ps(int argc, char **argv) { (void)argc; (void)argv; sched_ps(); }

static void c_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t ms = os_uptime_ms(), s = ms / 1000u;
    kprintf("up %u:%02u:%02u.%03u\n", (unsigned)(s / 3600u), (unsigned)((s / 60u) % 60u),
            (unsigned)(s % 60u), (unsigned)(ms % 1000u));
}

static void c_info(int argc, char **argv) { (void)argc; (void)argv; os_info(); }

static void c_peek(int argc, char **argv)
{
    if (argc < 2) { kprintf("usage: peek HEXADDR [WORDS]\n"); return; }
    uint32_t a = 0, n = 4;
    const char *s = argv[1];
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s; s++) {
        int d = (*s >= '0' && *s <= '9') ? *s - '0' : (*s >= 'a' && *s <= 'f') ? *s - 'a' + 10 :
                (*s >= 'A' && *s <= 'F') ? *s - 'A' + 10 : -1;
        if (d < 0) { kprintf("bad address\n"); return; }
        a = (a << 4) | (uint32_t)d;
    }
    if (argc > 2) { n = 0; for (const char *p = argv[2]; *p >= '0' && *p <= '9'; p++) n = n * 10 + (uint32_t)(*p - '0'); }
    if (n > 64) n = 64;
    a &= ~3u;
    for (uint32_t i = 0; i < n; i++) {
        if (i % 4 == 0) kprintf("%s%08x:", i ? "\n" : "", (unsigned)(a + i * 4));
        kprintf(" %08x", (unsigned)*(volatile uint32_t *)(uintptr_t)(a + i * 4));
    }
    kprintf("\n");
}

static void c_lcd(int argc, char **argv)
{
    if (argc < 2) { kprintf("usage: lcd on|off\n"); return; }
    os_lcd_enable(!strcmp(argv[1], "on"));
}

static void c_reboot(int argc, char **argv) { (void)argc; (void)argv; os_reboot(); }

typedef struct { const char *name; const char *help; void (*fn)(int, char **); } cmd_t;
static const cmd_t cmds[] = {
    { "help",   "list commands",               c_help },
    { "echo",   "echo TEXT",                   c_echo },
    { "clear",  "clear the screen",            c_clear },
    { "ls",     "ls [DIR]  list files",        c_ls },
    { "cd",     "cd DIR",                      c_cd },
    { "pwd",    "show current directory",      c_pwd },
    { "cat",    "cat FILE  show a text file",  c_cat },
    { "mount",  "(re)read the SD card",        c_mount },
    { "mem",    "heap usage",                  c_mem },
    { "ps",     "list tasks",                  c_ps },
    { "uptime", "time since boot",             c_uptime },
    { "info",   "system information",          c_info },
    { "peek",   "peek HEXADDR [WORDS]",        c_peek },
    { "lcd",    "lcd on|off",                  c_lcd },
    { "reboot", "restart the machine",         c_reboot },
};
#define NCMDS ((int)(sizeof cmds / sizeof cmds[0]))

static void c_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    for (int i = 0; i < NCMDS; i++) kprintf("%-7s %s\n", cmds[i].name, cmds[i].help);
}

int shell_exec(char *line)
{
    char *argv[MAX_ARGS];
    int argc = 0;
    char *p = line;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"') p++;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        if (*p) *p++ = 0;
    }
    if (argc == 0) return 0;
    for (int i = 0; i < NCMDS; i++) {
        if (!strcmp(argv[0], cmds[i].name)) { cmds[i].fn(argc, argv); return 0; }
    }
    kprintf("unknown command '%s' (try help)\n", argv[0]);
    return -1;
}
