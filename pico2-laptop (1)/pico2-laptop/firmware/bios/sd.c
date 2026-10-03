/* microSD card in SPI mode, read-only.  Shares SPI0 with the LCD. */
#include "bios.h"
#include "irq.h"

static int sdhc;
static uint32_t blocks;
static int ready;

#define SD_SLOW_HZ 295000u
#define SD_FAST_HZ 25000000u

static void cs_lo(void) { gpio_lo(PIN_SD_CS); }
static void cs_hi(void) { gpio_hi(PIN_SD_CS); spi0_xfer(0xff); }

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    spi0_xfer(0xff);
    spi0_xfer(0x40 | cmd);
    spi0_xfer((uint8_t)(arg >> 24)); spi0_xfer((uint8_t)(arg >> 16));
    spi0_xfer((uint8_t)(arg >> 8));  spi0_xfer((uint8_t)arg);
    spi0_xfer(crc);
    for (int i = 0; i < 10; i++) {
        uint8_t r = spi0_xfer(0xff);
        if (!(r & 0x80)) return r;
    }
    return 0xff;
}

static int wait_ready(uint32_t ms)
{
    uint32_t t0 = millis();
    while (spi0_xfer(0xff) != 0xff)
        if ((uint32_t)(millis() - t0) > ms) return -1;
    return 0;
}

/* read_csd: only CSD v2 (SDHC/SDXC) gives a size */
static void read_capacity(void)
{
    blocks = 0;
    cs_lo();
    if (sd_cmd(9, 0, 0xff) == 0) {
        uint32_t t0 = millis();
        uint8_t tok;
        while ((tok = spi0_xfer(0xff)) == 0xff && (uint32_t)(millis() - t0) < 100) { }
        if (tok == 0xfe) {
            uint8_t csd[16];
            spi0_read(csd, 16);
            spi0_xfer(0xff); spi0_xfer(0xff);
            if ((csd[0] >> 6) == 1) {
                uint32_t c_size = ((uint32_t)(csd[7] & 0x3f) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
                blocks = (c_size + 1u) * 1024u;
            }
        }
    }
    cs_hi();
}

int sd_init(void)
{
    uint32_t irq = irq_save();
    int rc = -1;
    ready = 0;
    gpio_hi(PIN_LCD_CS);
    gpio_hi(PIN_SD_CS);
    spi0_set_speed(SD_SLOW_HZ);
    for (int i = 0; i < 10; i++) spi0_xfer(0xff);          /* 80 clocks, card unselected */

    cs_lo();
    uint8_t r = 0xff;
    for (int i = 0; i < 5 && r != 0x01; i++) r = sd_cmd(0, 0, 0x95);
    if (r != 0x01) { rc = -2; goto out; }                  /* no card / not responding */

    int v2 = 0;
    r = sd_cmd(8, 0x1aa, 0x87);
    if (r == 0x01) {
        uint8_t ocr[4];
        spi0_read(ocr, 4);
        if (ocr[2] != 0x01 || ocr[3] != 0xaa) { rc = -3; goto out; }
        v2 = 1;
    }
    uint32_t t0 = millis();
    for (;;) {
        sd_cmd(55, 0, 0xff);
        r = sd_cmd(41, v2 ? 0x40000000u : 0, 0xff);
        if (r == 0) break;
        if ((uint32_t)(millis() - t0) > 1000) { rc = -4; goto out; }
        delay_ms(1);
    }
    sdhc = 0;
    if (v2) {
        if (sd_cmd(58, 0, 0xff) == 0) {
            uint8_t ocr[4];
            spi0_read(ocr, 4);
            sdhc = (ocr[0] & 0x40) ? 1 : 0;
        }
    }
    if (!sdhc) sd_cmd(16, 512, 0xff);
    cs_hi();
    spi0_set_speed(SD_FAST_HZ);
    read_capacity();
    ready = 1;
    rc = 0;
    irq_restore(irq);
    return rc;
out:
    cs_hi();
    irq_restore(irq);
    return rc;
}

int sd_read_block(uint32_t lba, void *buf)
{
    if (!ready) return -1;
    uint32_t irq = irq_save();
    int rc = -2;
    gpio_hi(PIN_LCD_CS);
    spi0_set_speed(SD_FAST_HZ);
    cs_lo();
    if (wait_ready(100) == 0 && sd_cmd(17, sdhc ? lba : lba * 512u, 0xff) == 0) {
        uint32_t t0 = millis();
        uint8_t tok;
        while ((tok = spi0_xfer(0xff)) == 0xff && (uint32_t)(millis() - t0) < 200) { }
        if (tok == 0xfe) {
            spi0_read((uint8_t *)buf, 512);
            spi0_xfer(0xff); spi0_xfer(0xff);              /* CRC, ignored */
            rc = 0;
        }
    }
    cs_hi();
    irq_restore(irq);
    return rc;
}

uint32_t sd_block_count(void) { return ready ? blocks : 0; }
