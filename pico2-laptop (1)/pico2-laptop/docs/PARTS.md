# Parts list

Costs are my rough guesses and are NOT checked against current prices. Look
them up before you buy. Only buy for the phase you are on.

## Phase 0-1: a Pico 2 and a cable
| Part | Qty | Notes |
|---|---|---|
| Raspberry Pi Pico 2 **with headers** (the "H" version) | 1 | RP2350. Not the Pico 2 W unless you want Wi-Fi later; the LED works differently on the W. |
| Micro-USB cable (data, not charge-only) | 1 | |
| 3.3 V USB-to-serial adapter (CP2102, FT232 or similar) | 1 | Must be 3.3 V logic. Needed for the console. |
| Breadboard, 830 points | 1-2 | |
| Jumper wires (male-male and male-female) | 1 pack | |

## Phase 2: screen, storage, keys
| Part | Qty | Notes |
|---|---|---|
| ILI9341 SPI TFT, 2.4" to 2.8", 320x240 | 1 | No touch needed. Check it has an SPI interface. |
| microSD socket breakout (3.3 V safe) | 1 | See WIRING.md warning |
| microSD card, 32 GB or less | 1 | A spare, not your photos. |
| Tactile switches or keyboard switches | 5 to start, 63 later | |
| 1N4148 diodes | 70 | One per key |
| Perfboard or a laser-cut/3D-printed plate, thin wire | 1 | For hand-wiring the matrix |
| Keycaps | 63 | Optional for now |

## Phase 4 (later, not needed yet)
| Part | Notes |
|---|---|
| RP2350A chip (QFN-60) | Same pins as the Pico 2, so firmware does not change |
| 16 MB SPI flash, 12 MHz crystal | Copy the Pico 2 reference design |
| 1-cell LiPo battery + charger chip | Choose after you measure real current draw |
| USB-C connector, 3.3 V regulator | |
| Display connector, microSD socket, keyboard connector | |

Rough total for Phases 0-3: a small fraction of your $500 budget.
Everything in Phases 0-3 is reusable on the final board.
