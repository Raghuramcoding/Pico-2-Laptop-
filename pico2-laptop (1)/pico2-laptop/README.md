# Pico 2 Laptop -- your own BIOS and your own OS

A small laptop-style computer you build in steps. You write the BIOS and the
OS. Nothing here uses someone else's OS or the Pico SDK.

**The chip:** RP2350 (the chip on the Raspberry Pi Pico 2).
Why not the Pi Zero 2? Its main chip is closed and has no BIOS you can replace.
The RP2350 is fully documented, so the BIOS and OS can be 100% yours.

**What this is:** a fast, text-first computer. It has a screen, a keyboard, an
SD card and a shell. It does not run a web browser or Linux apps. The RP2350
has no memory protection unit (MMU) and about 520 KB of RAM.

## Start here
1. Read `docs/ROADMAP.md` (the plan, as checklists).
2. Buy parts from `docs/PARTS.md` (Phase 0 needs only a Pico 2 and a cable).
3. Build and flash Phase 0 (below). Do not skip it.
4. Wire things up with `docs/WIRING.md`, one phase at a time.
5. When Phase 3 works, order the PCB using `docs/PCB_QUOTE.md`.

## No board yet? Run it in a VM
See `docs/VM.md`. Short version: install QEMU, then `python3 tools/run_vm.py --screen`.
The real OS runs on an emulated Cortex-M33 with a virtual SD card and a virtual LCD.

## Quick try (no tools needed)
The folder `prebuilt/` has ready-made files. Flash `phase0.uf2` first (see "Flash it"). Later use `laptop.uf2`.

## Build it
You need: `arm-none-eabi-gcc` (version 12 or newer), `make`, `python3`.

- Ubuntu/Debian/WSL: `sudo apt install gcc-arm-none-eabi make python3`
- Windows: install the "Arm GNU Toolchain" and `make`, or use WSL.

```
cd firmware
make phase0        # -> build/phase0.uf2   (LED blink + serial hello)
make               # -> build/laptop.uf2   (BIOS + OS together)
```

## Flash it
1. Unplug the Pico 2. Hold the **BOOTSEL** button. Plug in USB. Let go.
2. A drive named **RP2350** appears.
3. Copy the `.uf2` file onto it. The Pico restarts by itself.

If drag-and-drop fails, use `picotool load -x build/laptop.uf2`.

## Serial console
Connect a 3.3 V USB-serial adapter: adapter RX to GP0, adapter TX to GP1,
GND to GND. Open a terminal at **115200, 8N1** (`screen /dev/ttyUSB0 115200`
or PuTTY/Tera Term). The BIOS and the OS both talk here. You can do all of
Phases 0, 1 and 3 with only this cable -- no screen, SD card or keyboard needed.

## What is in the box
```
firmware/
  phase0/    Phase 0: blink + clocks + serial hello
  bios/      Phase 1-2: the BIOS (boot, monitor, LCD, SD, keyboard drivers)
  os/        Phase 3: the OS (scheduler, heap, FAT, terminal, shell)
  common/    shared: startup, clocks, UART, SPI, printf, keyboard logic
  tests/     tests that run on your PC (no hardware)
  qemu/      runs the real OS on an emulated Cortex-M33 chip
tools/mkuf2.py   makes the .uf2 files
tools/run_vm.py  runs the OS in a VM (QEMU), with a virtual LCD and SD card
tools/make_disk.py  makes a virtual SD card image from a folder
docs/            roadmap, parts, wiring, PCB quote, hardware checklist
```

## What was tested, and what was not
Tested on a PC / in an emulator:
- Everything compiles with no warnings.
- `make test`: heap, printf, paths, keyboard logic, shell, and the FAT driver
  against real FAT16, FAT32 and partitioned disk images (1,000+ checks).
- `make qemu-test` and `tools/run_vm.py`: the real OS boots on an emulated Cortex-M33. The scheduler
  switches tasks (even while one task never gives up the CPU), sleeping tasks
  wake on time, and the shell answers commands.

**NOT tested on a real Pico 2 -- I had no hardware.** The chip-specific code
(clocks, UART, SPI, display, SD, keyboard pins, boot block) was written from
the RP2350 datasheet. Expect to fix a few things. `docs/VERIFY_ON_HARDWARE.md`
lists the risky spots in order and says what each failure looks like.
Send me the exact symptom and I will fix it.
