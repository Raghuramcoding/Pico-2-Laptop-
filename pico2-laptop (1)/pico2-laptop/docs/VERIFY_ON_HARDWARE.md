# Check these on real hardware (in this order)

I could not test on a real Pico 2. The chip-specific code was written from
the RP2350 datasheet. Everything else (heap, FAT, shell, keyboard logic,
scheduler) was tested on a PC or in an emulator. Work down this list. Stop at
the first failure and send me the symptom.

## 1. Does the chip run my code? (phase0.uf2)
- [ ] LED blinks 5 times fast.
  - Nothing happens: the boot block may be wrong (`image_def` in
    `common/startup.c`), or the .uf2 family ID in `tools/mkuf2.py`.
    Try `picotool info -a build/phase0.uf2` (shows if it is accepted).
  - Also check the board is a Pico 2 (RP2350), not a Pico 1.
## 2. Do the clocks start?
- [ ] After the fast blinks, LED goes 1 blink per second.
  - N slow blinks then a pause: 1 = crystal, 2 = PLL, 3 = clock switch, 4 = reset bit.
    These point at `clocks_init()` in `common/hal.c`.
  - Blinks but not at 1 per second: the PLL numbers or timer tick are off.
## 3. Does serial work?
- [ ] `Phase 0 OK` and `tick N` lines at 115200.
  - Garbage text: clock speed is wrong, or baud mismatch.
  - Nothing: check TX/RX are not swapped, and GND is shared.
## 4. Does the BIOS start? (bios_only.uf2)
- [ ] Banner, CPU 150 MHz, `RAM 488 KB ok`.
  - `RAM FAIL` + 5 blinks: the RAM map in `bios_api.h` is wrong.
## 5. Display
- [ ] `lcd` command shows colour bars.
  - Blank white: reset or backlight wiring.
  - Garbled: SPI speed is too high for your wires. Lower `LCD_HZ` in `bios/lcd.c`.
  - Wrong colours: the panel needs RGB not BGR (bit 3 of `i_36`).
  - Mirrored or upside down: change `i_36` (0x28, 0xE8, 0x48, 0x88).
  - Some cheap boards are ST7789, not ILI9341: needs another init sequence.
## 6. SD card
- [ ] `sd` prints `ok` and a size.
  - -2 = card not answering: check wiring, 3.3 V, card <= 32 GB.
  - Works alone but breaks when the display is connected: display SDO/MISO
    must be unconnected.
## 7. Keyboard
- [ ] `kbd` prints codes. If keys are missing or doubled, check diode direction.
## 8. OS (laptop.uf2)
- [ ] BIOS counts down and starts the OS; prompt appears. `ps` shows tasks.
- [ ] `mount`, `ls`, `cat` work on a FAT32 card.
## 9. Known loose ends
- `reboot` uses a software reset request. If it hangs, unplug USB. Tell me.
- While the screen is drawing a row or the SD card reads a block, interrupts
  are off for up to about a millisecond. The system tick can lag a little.
- Only short 8.3 file names work (a long name still has a short name; `ls`
  shows it).
- SD is read-only on purpose. Writing needs care so cards are not damaged.
- If hardfaults happen, the serial line prints the fault address (pc=...).
  Use `arm-none-eabi-addr2line -e build/os.elf 0xADDRESS` to find the line.
