#ifndef IRQ_H
#define IRQ_H
#include <stdint.h>
#if defined(__arm__)
static inline uint32_t irq_save(void)
{
    uint32_t p;
    __asm volatile("mrs %0, primask\n cpsid i" : "=r"(p) :: "memory");
    return p;
}
static inline void irq_restore(uint32_t p)
{
    __asm volatile("msr primask, %0" :: "r"(p) : "memory");
}
#else  /* host unit tests */
static inline uint32_t irq_save(void) { return 0; }
static inline void irq_restore(uint32_t p) { (void)p; }
#endif
#endif
