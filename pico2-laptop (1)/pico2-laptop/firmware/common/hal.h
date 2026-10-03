#ifndef HAL_H
#define HAL_H
#include "rp2350.h"

/* Failure codes (LED blinks this many times, then pauses) */
#define FAIL_XOSC   1
#define FAIL_PLL    2
#define FAIL_CLKSEL 3
#define FAIL_RESET  4

int  clocks_init(void);               /* 0 = ok, else FAIL_* */
void fail_blink(int code);            /* never returns */
bool reset_release(uint32_t mask);

void timer_init(void);
uint32_t micros(void);
uint32_t millis(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

void uart_init(uint32_t baud);
void uart_putc(char c);
int  uart_getc(void);                 /* -1 if nothing waiting */
void uart_flush(void);

/* SPI0 shared by the LCD and the SD card (pins in WIRING.md) */
#define PIN_SPI_MISO 16u
#define PIN_SPI_SCK  18u
#define PIN_SPI_MOSI 19u
void spi0_init(void);
void spi0_set_speed(uint32_t hz);
uint8_t spi0_xfer(uint8_t b);
void spi0_write(const uint8_t *p, uint32_t n);
void spi0_read(uint8_t *p, uint32_t n);
void spi0_flush(void);
#endif
