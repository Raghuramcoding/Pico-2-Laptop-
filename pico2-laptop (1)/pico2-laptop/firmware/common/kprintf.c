/* Tiny printf: %c %s %d %i %u %x %X %p %% with width, '-' and '0' flags. */
#include "kprintf.h"
#include <stdint.h>

static kputc_fn g_out;
void kprintf_set_out(kputc_fn f) { g_out = f; }

typedef struct { kputc_fn fn; char *buf; size_t cap; size_t n; } sink_t;

static void sput(sink_t *s, char c)
{
    if (s->fn) s->fn(c);
    else if (s->buf && s->n + 1 < s->cap) s->buf[s->n] = c;
    s->n++;
}
static void spad(sink_t *s, int count, char pad) { while (count-- > 0) sput(s, pad); }

static void snum(sink_t *s, unsigned v, unsigned base, int upper, int neg,
                 int width, int left, char pad)
{
    char tmp[12];
    int i = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (v == 0) tmp[i++] = '0';
    while (v) { tmp[i++] = dig[v % base]; v /= base; }
    int len = i + (neg ? 1 : 0);
    int padn = width > len ? width - len : 0;
    if (!left && pad == ' ') spad(s, padn, ' ');
    if (neg) sput(s, '-');
    if (!left && pad == '0') spad(s, padn, '0');
    while (i) sput(s, tmp[--i]);
    if (left) spad(s, padn, ' ');
}

static int core(sink_t *s, const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        if (*fmt != '%') { sput(s, *fmt); continue; }
        fmt++;
        int left = 0, width = 0;
        char pad = ' ';
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') pad = '0';
            else break;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            fmt++;
            if (width < 0) { left = 1; width = -width; }
        } else {
            while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
        }
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z') fmt++;
        if (!*fmt) break;
        switch (*fmt) {
        case 'd': case 'i': {
            int v = va_arg(ap, int);
            unsigned u = v < 0 ? (unsigned)(-(v + 1)) + 1u : (unsigned)v;
            snum(s, u, 10, 0, v < 0, width, left, pad);
            break; }
        case 'u': snum(s, va_arg(ap, unsigned), 10, 0, 0, width, left, pad); break;
        case 'x': snum(s, va_arg(ap, unsigned), 16, 0, 0, width, left, pad); break;
        case 'X': snum(s, va_arg(ap, unsigned), 16, 1, 0, width, left, pad); break;
        case 'p':
            sput(s, '0'); sput(s, 'x');
            snum(s, (unsigned)(uintptr_t)va_arg(ap, void *), 16, 0, 0, 8, 0, '0');
            break;
        case 'c': {
            char c = (char)va_arg(ap, int);
            if (!left) spad(s, width - 1, ' ');
            sput(s, c);
            if (left) spad(s, width - 1, ' ');
            break; }
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            int len = 0;
            while (str[len]) len++;
            if (!left) spad(s, width - len, ' ');
            for (int i = 0; i < len; i++) sput(s, str[i]);
            if (left) spad(s, width - len, ' ');
            break; }
        case '%': sput(s, '%'); break;
        default: sput(s, '%'); sput(s, *fmt); break;
        }
    }
    return (int)s->n;
}

int kprintf(const char *fmt, ...)
{
    sink_t s = { g_out, 0, 0, 0 };
    va_list ap;
    va_start(ap, fmt);
    int r = core(&s, fmt, ap);
    va_end(ap);
    return r;
}

int ksnprintf(char *buf, size_t n, const char *fmt, ...)
{
    sink_t s = { 0, buf, n, 0 };
    va_list ap;
    va_start(ap, fmt);
    int r = core(&s, fmt, ap);
    va_end(ap);
    if (n) buf[s.n < n ? s.n : n - 1] = 0;
    return r;
}

void kputs(const char *s) { while (*s) { if (g_out) g_out(*s); s++; } }
