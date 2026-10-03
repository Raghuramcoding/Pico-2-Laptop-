#ifndef KPRINTF_H
#define KPRINTF_H
#include <stdarg.h>
#include <stddef.h>

typedef void (*kputc_fn)(char);

void kprintf_set_out(kputc_fn f);
int  kprintf(const char *fmt, ...);
int  ksnprintf(char *buf, size_t n, const char *fmt, ...);
void kputs(const char *s);
#endif
