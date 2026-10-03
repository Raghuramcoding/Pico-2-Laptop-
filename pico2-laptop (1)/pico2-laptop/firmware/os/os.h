#ifndef OS_H
#define OS_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kprintf.h"
#include "irq.h"
#include "fat.h"

#define OS_NAME "PicoOS 0.1"

/* mem.c -- first-fit heap with coalescing */
typedef struct { uint32_t used, free, blocks_used, blocks_free, largest_free; } heap_stats_t;
void  heap_init(void *start, void *end);
void *kmalloc(size_t n);
void  kfree(void *p);
void  heap_stats(heap_stats_t *s);

/* sched.c -- round-robin preemptive scheduler, 1 ms tick */
int      task_create(const char *name, void (*fn)(void *), void *arg, uint32_t stack_bytes);
void     sched_init(void);
void     sched_start(void) __attribute__((noreturn));
void     task_sleep(uint32_t ms);
uint32_t sys_ticks(void);
void     sched_ps(void);

/* term.c -- 40x30 text terminal drawn on the LCD */
void term_init(void);
void term_putc(char c);
void term_clear(void);
void term_flush(void);

/* pathutil.c */
int path_resolve(const char *cwd, const char *arg, char *out, size_t outsz);

/* shell.c */
void        shell_init(void);
int         shell_exec(char *line);
const char *shell_cwd(void);

/* services the shell needs from os_main.c (stubbed in the host tests) */
uint32_t  os_uptime_ms(void);
void      os_reboot(void);
void      os_lcd_enable(int on);
void      os_info(void);
fat_fs_t *os_fs(void);          /* NULL when no file system is mounted */
int       os_mount(void);       /* 0 = ok */
#endif
