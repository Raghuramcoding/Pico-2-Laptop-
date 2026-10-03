/* Simple first-fit heap.  Blocks sit back to back; each has an 8-byte header.
 * kfree() merges neighbouring free blocks with one pass over the list. */
#include "os.h"
#include <string.h>

typedef struct { uint32_t size; uint32_t free; } blk_t;   /* size = payload bytes */
#define HDR ((uint32_t)sizeof(blk_t))

static uint8_t *lo, *hi;

void heap_init(void *start, void *end)
{
    uintptr_t s = ((uintptr_t)start + 7u) & ~(uintptr_t)7;
    uintptr_t e = (uintptr_t)end & ~(uintptr_t)7;
    lo = (uint8_t *)s; hi = (uint8_t *)e;
    blk_t *b = (blk_t *)lo;
    b->size = (uint32_t)(e - s) - HDR;
    b->free = 1;
}

void *kmalloc(size_t n)
{
    if (n == 0) n = 8;
    n = (n + 7u) & ~(size_t)7u;
    uint32_t irq = irq_save();
    void *res = NULL;
    for (uint8_t *p = lo; p + HDR <= hi; ) {
        blk_t *b = (blk_t *)p;
        if (b->free && b->size >= n) {
            if (b->size >= n + HDR + 8) {              /* split */
                blk_t *rest = (blk_t *)(p + HDR + n);
                rest->size = b->size - (uint32_t)n - HDR;
                rest->free = 1;
                b->size = (uint32_t)n;
            }
            b->free = 0;
            res = p + HDR;
            break;
        }
        if (b->size == 0 || b->size > (uint32_t)(hi - p)) break;   /* corrupt */
        p += HDR + b->size;
    }
    irq_restore(irq);
    return res;
}

void kfree(void *ptr)
{
    if (!ptr) return;
    uint32_t irq = irq_save();
    blk_t *b = (blk_t *)((uint8_t *)ptr - HDR);
    b->free = 1;
    for (uint8_t *p = lo; p + HDR <= hi; ) {           /* coalesce */
        blk_t *c = (blk_t *)p;
        uint8_t *nx = p + HDR + c->size;
        if (c->free && nx + HDR <= hi && ((blk_t *)nx)->free) {
            c->size += HDR + ((blk_t *)nx)->size;
            continue;                                  /* try to merge again */
        }
        p = nx;
    }
    irq_restore(irq);
}

void heap_stats(heap_stats_t *s)
{
    memset(s, 0, sizeof *s);
    uint32_t irq = irq_save();
    for (uint8_t *p = lo; p + HDR <= hi; ) {
        blk_t *b = (blk_t *)p;
        if (b->free) {
            s->free += b->size; s->blocks_free++;
            if (b->size > s->largest_free) s->largest_free = b->size;
        } else {
            s->used += b->size; s->blocks_used++;
        }
        p += HDR + b->size;
    }
    irq_restore(irq);
}
