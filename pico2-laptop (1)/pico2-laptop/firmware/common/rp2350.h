/* Minimal RP2350 register map -- only what this project uses.
 * Addresses come from the RP2350 datasheet memory map. */
#ifndef RP2350_H
#define RP2350_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))

#define CLOCKS_BASE     0x40010000u
#define RESETS_BASE     0x40020000u
#define IO_BANK0_BASE   0x40028000u
#define PADS_BANK0_BASE 0x40038000u
#define XOSC_BASE       0x40048000u
#define PLL_SYS_BASE    0x40050000u
#define UART0_BASE      0x40070000u
#define SPI0_BASE       0x40080000u
#define TIMER0_BASE     0x400b0000u
#define WATCHDOG_BASE   0x400d8000u
#define TICKS_BASE      0x40108000u
#define SIO_BASE        0xd0000000u

/* bits in the RESETS register */
#define RST_IO_BANK0    (1u << 6)
#define RST_PADS_BANK0  (1u << 9)
#define RST_PLL_SYS     (1u << 14)
#define RST_SPI0        (1u << 18)
#define RST_TIMER0      (1u << 23)
#define RST_UART0       (1u << 26)

/* Cortex-M33 system registers */
#define SCB_ICSR   0xE000ED04u
#define SCB_VTOR   0xE000ED08u
#define SCB_AIRCR  0xE000ED0Cu
#define SCB_SHPR3  0xE000ED20u
#define SCB_CFSR   0xE000ED28u
#define SCB_HFSR   0xE000ED2Cu
#define SYST_CSR   0xE000E010u
#define SYST_RVR   0xE000E014u
#define SYST_CVR   0xE000E018u

#define CPU_HZ     150000000u   /* clk_sys after clocks_init() */
#define XOSC_HZ    12000000u

/* ---- GPIO ---- */
#define PAD_SLEWFAST (1u << 0)
#define PAD_SCHMITT  (1u << 1)
#define PAD_PDE      (1u << 2)
#define PAD_PUE      (1u << 3)
#define PAD_DRIVE_8  (2u << 4)
#define PAD_IE       (1u << 6)
/* bit 8 (ISO) and bit 7 (OD) stay 0: that un-isolates the pad */

#define FN_SPI  1u
#define FN_UART 2u
#define FN_SIO  5u

#define SIO_GPIO_IN      (SIO_BASE + 0x04u)
#define SIO_GPIO_OUT_SET (SIO_BASE + 0x18u)
#define SIO_GPIO_OUT_CLR (SIO_BASE + 0x20u)
#define SIO_GPIO_OE_SET  (SIO_BASE + 0x38u)
#define SIO_GPIO_OE_CLR  (SIO_BASE + 0x40u)

static inline void gpio_config(unsigned pin, unsigned fn, uint32_t pad)
{
    REG(PADS_BANK0_BASE + 4u + 4u * pin) = pad;
    REG(IO_BANK0_BASE + 8u * pin + 4u) = fn;
}
static inline void gpio_hi(unsigned pin) { REG(SIO_GPIO_OUT_SET) = 1u << pin; }
static inline void gpio_lo(unsigned pin) { REG(SIO_GPIO_OUT_CLR) = 1u << pin; }
static inline bool gpio_read(unsigned pin) { return (REG(SIO_GPIO_IN) >> pin) & 1u; }
static inline void gpio_out_init(unsigned pin, bool level)
{
    if (level) gpio_hi(pin); else gpio_lo(pin);
    gpio_config(pin, FN_SIO, PAD_IE | PAD_SCHMITT);
    REG(SIO_GPIO_OE_SET) = 1u << pin;
}
static inline void gpio_in_init(unsigned pin, uint32_t pull)
{
    REG(SIO_GPIO_OE_CLR) = 1u << pin;
    gpio_config(pin, FN_SIO, PAD_IE | PAD_SCHMITT | pull);
}

#define PIN_LED 25u   /* on-board LED of the Pico 2 */

#endif
