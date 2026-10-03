/* Preemptive round-robin scheduler for one Cortex-M33 core.
 * SysTick fires every 1 ms and asks for a PendSV; PendSV saves the running
 * task's registers on its own stack (PSP), picks the next ready task and
 * restores that one.  Tasks sleep with task_sleep(). */
#include "os.h"
#include "rp2350.h"
#include "bios_api.h"
#include <string.h>

#define MAX_TASKS 8
#define STACK_FILL 0xA5A5A5A5u

typedef enum { T_FREE = 0, T_READY, T_SLEEP, T_DEAD } tstate_t;

typedef struct task {
    uint32_t *sp;                 /* MUST stay the first member (used by asm) */
    const char *name;
    tstate_t state;
    uint32_t wake;
    uint32_t *stack;
    uint32_t stack_words;
    uint32_t ticks;
} task_t;

static task_t tasks[MAX_TASKS];
static int ntasks;
task_t *g_current;                /* read by the SVC handler (asm) */
static volatile uint32_t tick_ms;

uint32_t sys_ticks(void) { return tick_ms; }

static void task_exit(void)
{
    g_current->state = T_DEAD;
    for (;;) task_sleep(1000);
}

int task_create(const char *name, void (*fn)(void *), void *arg, uint32_t stack_bytes)
{
    uint32_t irq = irq_save();
    if (ntasks >= MAX_TASKS) { irq_restore(irq); return -1; }
    uint32_t words = stack_bytes / 4u;
    uint32_t *stk = kmalloc(stack_bytes);
    if (!stk) { irq_restore(irq); return -2; }
    for (uint32_t i = 0; i < words; i++) stk[i] = STACK_FILL;

    uint32_t *sp = stk + words;       /* top, 8-byte aligned (kmalloc aligns to 8) */
    sp -= 8;                          /* hardware exception frame */
    sp[0] = (uint32_t)(uintptr_t)arg; /* r0 */
    sp[1] = sp[2] = sp[3] = sp[4] = 0;/* r1 r2 r3 r12 */
    sp[5] = (uint32_t)(uintptr_t)task_exit;   /* lr */
    sp[6] = (uint32_t)(uintptr_t)fn;          /* pc */
    sp[7] = 0x01000000u;              /* xPSR: Thumb bit */
    sp -= 8;                          /* r4..r11 saved by PendSV */
    for (int i = 0; i < 8; i++) sp[i] = 0;

    task_t *t = &tasks[ntasks];
    t->sp = sp; t->name = name; t->state = T_READY; t->wake = 0;
    t->stack = stk; t->stack_words = words; t->ticks = 0;
    int id = ntasks++;
    irq_restore(irq);
    return id;
}

void sched_init(void)
{
    memset(tasks, 0, sizeof tasks);
    ntasks = 0; g_current = NULL; tick_ms = 0;
}

static task_t *pick_next(void)
{
    int cur = g_current ? (int)(g_current - tasks) : 0;
    for (int i = 1; i <= ntasks; i++) {
        int idx = (cur + i) % ntasks;
        task_t *t = &tasks[idx];
        if (idx == 0) continue;                       /* task 0 = idle: last resort */
        if (t->state == T_SLEEP && (int32_t)(tick_ms - t->wake) >= 0) t->state = T_READY;
        if (t->state == T_READY) return t;
    }
    return &tasks[0];
}

uint32_t *sched_switch(uint32_t *old_sp)
{
    g_current->sp = old_sp;
    g_current = pick_next();
    return g_current->sp;
}

void SysTick_Handler(void)
{
    tick_ms++;
    if (g_current) g_current->ticks++;
    REG(SCB_ICSR) = 1u << 28;                         /* pend PendSV */
}

__attribute__((naked)) void PendSV_Handler(void)
{
    __asm volatile(
        "mrs   r0, psp\n"
        "stmdb r0!, {r4-r11}\n"
        "push  {r3, lr}\n"
        "bl    sched_switch\n"
        "pop   {r3, lr}\n"
        "ldmia r0!, {r4-r11}\n"
        "msr   psp, r0\n"
        "bx    lr\n");
}

__attribute__((naked)) void SVC_Handler(void)         /* starts the first task */
{
    __asm volatile(
        "ldr   r0, =g_current\n"
        "ldr   r1, [r0]\n"
        "ldr   r0, [r1]\n"
        "ldmia r0!, {r4-r11}\n"
        "msr   psp, r0\n"
        "ldr   lr, =0xFFFFFFFD\n"
        "bx    lr\n");
}

void sched_start(void)
{
    g_current = &tasks[0];
    REG(SCB_SHPR3) |= 0xFFFF0000u;                    /* PendSV + SysTick: lowest priority */
    REG(SYST_RVR) = BIOS->cpu_hz / 1000u - 1u;
    REG(SYST_CVR) = 0;
    REG(SYST_CSR) = 7u;                               /* enable, interrupt, cpu clock */
    __asm volatile("svc 0");
    for (;;) { }
}

void task_sleep(uint32_t ms)
{
    uint32_t irq = irq_save();
    g_current->state = ms ? T_SLEEP : T_READY;
    g_current->wake = tick_ms + ms;
    irq_restore(irq);
    REG(SCB_ICSR) = 1u << 28;                         /* switch now */
    __asm volatile("dsb\n isb");
}

void sched_ps(void)
{
    static const char *const names[] = { "free", "ready", "sleep", "dead" };
    uint32_t total = tick_ms ? tick_ms : 1;
    kprintf("ID NAME      STATE  CPU%%  STACK FREE\n");
    for (int i = 0; i < ntasks; i++) {
        task_t *t = &tasks[i];
        uint32_t freew = 0;
        while (freew < t->stack_words && t->stack[freew] == STACK_FILL) freew++;
        kprintf("%-2d %-9s %-6s %3u   %u/%u B\n", i, t->name, names[t->state],
                (unsigned)((t->ticks * 100u) / total), (unsigned)(freew * 4u),
                (unsigned)(t->stack_words * 4u));
    }
}
