# Roadmap

Pace: relaxed, about 4-6 months. Each phase has a "done when" test.
Do them in order. Each phase builds on the one before.

## Phase 0 -- Proof of life (1 weekend)
Goal: your code runs on the chip, with no SDK.
- [ ] Install the tools (see README)
- [ ] `make phase0`
- [ ] Flash `build/phase0.uf2`
- [ ] LED blinks 5 times fast
- [ ] Then it blinks exactly once per second
- [ ] Serial terminal shows `Phase 0 OK: clocks running at 150 MHz` and `tick N` lines

**Done when:** 60 blinks take 60 seconds on a stopwatch. That proves the
crystal and PLL are set up right.

LED patterns if something is wrong:
- Nothing at all: the boot block was not accepted (see VERIFY_ON_HARDWARE.md)
- N slow blinks, pause, repeat: clock setup failed. 1 = crystal, 2 = PLL, 3 = clock switch, 4 = reset release
- Very fast blinking forever: a hard fault (details print on serial)

## Phase 1 -- The BIOS (2-3 weeks)
Goal: a program that starts the hardware, checks it, and starts an OS.
- [ ] Flash `build/bios_only.uf2` (`make` creates it)
- [ ] Serial shows `Pico2 Laptop BIOS v0.1`, CPU speed and a RAM test result
- [ ] It says `OS image: NOT FOUND` and drops into `bios>`
- [ ] Try: `help`, `info`, `md 10000000 40` (dump flash), `md 20000000 40`
- [ ] Read `bios/bios_main.c` top to bottom. Change the banner. Re-flash.
- [ ] Flash `build/laptop.uf2` (BIOS + OS). It counts down 3 s, then starts the OS.
      Press a key during the countdown to get the BIOS monitor instead.

**Done when:** you can power on, see the BIOS banner, and either drop into the
monitor or boot the OS.

## Phase 2 -- Screen, SD card, keyboard (3-4 weeks)
Goal: the machine works without a PC attached.
- [ ] Wire the display (docs/WIRING.md). In the monitor: `lcd` shows 8 colour bars
- [ ] If the image is mirrored or upside down: change `i_36[]` in `bios/lcd.c` (0x28, 0xE8, 0x48, 0x88)
- [ ] Wire the SD card. In the monitor: `sd` prints `ok` and the size
- [ ] `sdr 0` dumps sector 0 (you should see `55 aa` at the end of a real card)
- [ ] Wire a few keys first (5 is plenty). In the monitor: `kbd` shows their codes
- [ ] Wire the full 63-key matrix (see the key table in WIRING.md) when you are happy

**Done when:** the BIOS status lines show on the screen, and `kbd` reports keys.

## Phase 3 -- The OS (4-6 weeks)
Goal: a small multitasking OS with files and a shell.
- [ ] Flash `build/laptop.uf2`
- [ ] Shell prompt appears on serial AND on the screen
- [ ] `help`, `echo`, `uptime`, `mem`, `ps`, `info`
- [ ] Put a few text files on a FAT32 SD card (short names like `NOTES.TXT`)
- [ ] `mount`, `ls`, `cd`, `cat`
- [ ] Read `os/sched.c` and `os/mem.c`. These are the heart of the OS.
- [ ] Add your own command in `os/shell.c` (hint: copy `c_pwd`)

**Done when:** you can type on the laptop keyboard, see text on the screen, and
read files from the SD card.

Ideas to grow it (pick what sounds fun):
- Load programs from the SD card (`RUN HELLO.BIN`)
- Write support for FAT (careful: this can damage cards, test on a spare)
- A text editor
- Long file names
- Sound, or a battery gauge
- Add 8 MB PSRAM (a Pico Plus 2 style board has it) for a bigger heap

## Phase 4 -- Your own board (4-8 weeks, only after Phase 3 works)
Goal: replace the Pico 2 and breadboard with one PCB.
- [ ] Read Raspberry Pi's "Hardware design with RP2350" guide (power, crystal, flash)
- [ ] Copy the Pico 2 reference design for the chip, crystal, flash and USB
- [ ] Keep the same pin numbers as WIRING.md so the firmware needs no changes
- [ ] Add: battery, charger, 3.3 V supply, USB-C, display connector, SD slot, keyboard connector
- [ ] Order a first run using `docs/PCB_QUOTE.md`
- [ ] Case: 3D print or hand-built
