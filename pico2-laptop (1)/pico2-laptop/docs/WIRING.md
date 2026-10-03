# Wiring

**Everything is 3.3 V. Never connect 5 V to a Pico pin.**
Pin numbers below are GP numbers (the names printed on the Pico) plus the
physical pin number on the 40-pin header (count from the USB end, left side
down, then right side up).

## Serial console (Phase 0 onward)
| Adapter | Pico 2 |
|---|---|
| RX | GP0 (pin 1) |
| TX | GP1 (pin 2) |
| GND | any GND (pin 3) |

## Display: ILI9341, 320x240, SPI (Phase 2)
| Display pin | Pico 2 |
|---|---|
| VCC | 3V3 OUT (pin 36) |
| GND | GND |
| CS | GP20 (pin 26) |
| RESET | GP22 (pin 29) |
| DC / RS | GP21 (pin 27) |
| SDI / MOSI | GP19 (pin 25) |
| SCK | GP18 (pin 24) |
| LED / BL (backlight) | 3V3 (through the resistor your module needs; check its sheet) |
| SDO / MISO | **leave unconnected** (the SD card owns the MISO line) |
| Touch pins | unused |

## microSD card: SPI (Phase 2)
| SD pin | Pico 2 |
|---|---|
| VCC | 3V3 OUT (pin 36) |
| GND | GND |
| CS | GP17 (pin 22) |
| MOSI / DI | GP19 (pin 25) -- shared with the display |
| SCK | GP18 (pin 24) -- shared with the display |
| MISO / DO | GP16 (pin 21) |

Use a module that works at 3.3 V logic. Cheap "5 V" modules with a level
shifter chip and a regulator often fail at 3.3 V. A bare microSD socket
breakout is the safest. Use a card of 32 GB or less, formatted FAT32.

## Keyboard matrix: 8 rows x 9 columns (Phase 2)
- Rows are driven by the Pico: **GP2 to GP9** (r0..r7)
- Columns are read by the Pico: **GP10-GP15 and GP26-GP28** (c0..c8)
- One **1N4148 diode per key**, in series with the switch:
  stripe (cathode) toward the **column** wire. This stops "ghost" keys.
- Do not use GP23, GP24, GP25 (not on the Pico 2 header).

Start with 5 keys to prove the idea. Then wire them all.

| Key | Row wire | Column wire |
|---|---|---|
| ` | r0 = GP2 | c0 = GP10 |
| 1 | r0 = GP2 | c1 = GP11 |
| 2 | r0 = GP2 | c2 = GP12 |
| 3 | r0 = GP2 | c3 = GP13 |
| 4 | r0 = GP2 | c4 = GP14 |
| 5 | r0 = GP2 | c5 = GP15 |
| 6 | r0 = GP2 | c6 = GP26 |
| Ctrl | r0 = GP2 | c7 = GP27 |
| Alt | r0 = GP2 | c8 = GP28 |
| 7 | r1 = GP3 | c0 = GP10 |
| 8 | r1 = GP3 | c1 = GP11 |
| 9 | r1 = GP3 | c2 = GP12 |
| 0 | r1 = GP3 | c3 = GP13 |
| - | r1 = GP3 | c4 = GP14 |
| = | r1 = GP3 | c5 = GP15 |
| Backspace | r1 = GP3 | c6 = GP26 |
| Space | r1 = GP3 | c7 = GP27 |
| Fn (reserved, not used yet) | r1 = GP3 | c8 = GP28 |
| Tab | r2 = GP4 | c0 = GP10 |
| q | r2 = GP4 | c1 = GP11 |
| w | r2 = GP4 | c2 = GP12 |
| e | r2 = GP4 | c3 = GP13 |
| r | r2 = GP4 | c4 = GP14 |
| t | r2 = GP4 | c5 = GP15 |
| y | r2 = GP4 | c6 = GP26 |
| Up arrow | r2 = GP4 | c7 = GP27 |
| Down arrow | r2 = GP4 | c8 = GP28 |
| u | r3 = GP5 | c0 = GP10 |
| i | r3 = GP5 | c1 = GP11 |
| o | r3 = GP5 | c2 = GP12 |
| p | r3 = GP5 | c3 = GP13 |
| [ | r3 = GP5 | c4 = GP14 |
| ] | r3 = GP5 | c5 = GP15 |
| \ (backslash) | r3 = GP5 | c6 = GP26 |
| Left arrow | r3 = GP5 | c7 = GP27 |
| Right arrow | r3 = GP5 | c8 = GP28 |
| Caps Lock | r4 = GP6 | c0 = GP10 |
| a | r4 = GP6 | c1 = GP11 |
| s | r4 = GP6 | c2 = GP12 |
| d | r4 = GP6 | c3 = GP13 |
| f | r4 = GP6 | c4 = GP14 |
| g | r4 = GP6 | c5 = GP15 |
| h | r4 = GP6 | c6 = GP26 |
| Delete | r4 = GP6 | c7 = GP27 |
| Esc | r4 = GP6 | c8 = GP28 |
| j | r5 = GP7 | c0 = GP10 |
| k | r5 = GP7 | c1 = GP11 |
| l | r5 = GP7 | c2 = GP12 |
| ; | r5 = GP7 | c3 = GP13 |
| ' (apostrophe) | r5 = GP7 | c4 = GP14 |
| Enter | r5 = GP7 | c5 = GP15 |
| Left Shift | r6 = GP8 | c0 = GP10 |
| z | r6 = GP8 | c1 = GP11 |
| x | r6 = GP8 | c2 = GP12 |
| c | r6 = GP8 | c3 = GP13 |
| v | r6 = GP8 | c4 = GP14 |
| b | r6 = GP8 | c5 = GP15 |
| n | r6 = GP8 | c6 = GP26 |
| m | r7 = GP9 | c0 = GP10 |
| , | r7 = GP9 | c1 = GP11 |
| . | r7 = GP9 | c2 = GP12 |
| / | r7 = GP9 | c3 = GP13 |
| Right Shift | r7 = GP9 | c4 = GP14 |

Physical pins for the matrix: rows GP2..GP9 = pins 4,5,6,7,9,10,11,12.
Columns GP10..GP15 = pins 14,15,16,17,19,20. GP26/27/28 = pins 31,32,34.

## Full pin summary
| GP | Use |
|---|---|
| 0, 1 | UART0 TX, RX |
| 2-9 | Keyboard rows |
| 10-15, 26-28 | Keyboard columns |
| 16 | SPI0 MISO (SD only) |
| 17 | SD chip select |
| 18, 19 | SPI0 SCK, MOSI (display + SD) |
| 20 | Display chip select |
| 21 | Display data/command |
| 22 | Display reset |
| 25 | On-board LED |
| 23, 24 | Internal to the Pico 2. Not used. |

## Power for Phases 0-3
Power the Pico 2 from USB. The display and SD card draw from its 3V3 pin.
(A display backlight plus SD card is fine on USB. Battery power is Phase 4.)
