# PCB quote sheet (for Phase 4)

You do not need Gerber files to get a price. Type these values into the
PCBWay instant quote (or any other fab). The board size is my estimate. Put in
your real outline once the layout is done and quote again.

## Main board
| Field | Enter |
|---|---|
| Board type | Single pieces |
| Different designs in panel | 1 |
| Size | 100 x 60 mm |
| Quantity | 5 |
| Layers | 4 |
| Material | FR-4, TG 130-140 |
| Thickness | 1.6 mm |
| Min track / spacing | 5/5 mil |
| Min hole size | 0.3 mm |
| Solder mask / silkscreen | Green / White |
| Surface finish | ENIG (best for the fine-pitch QFN chip) |
| Outer / inner copper | 1 oz / 0.5 oz |
| Impedance control | No |
| Via process | Tented vias, no via-in-pad |
| Edge plating, castellated holes, gold fingers | No |
| Build time | Standard |

Why 4 layers: the chip has fine-pitch pins, and flash and USB signals need a
solid ground layer.

## Optional keyboard board (separate quote)
150 x 60 mm, 2 layers, 1.6 mm, HASL lead-free, quantity 5.

## If you also want assembly
You will need: Gerbers, a BOM CSV and a pick-and-place (CPL) file, all from
KiCad. Keep every part on the top side to save money.

## If the price looks too high
The price for a 4-layer prototype run is mostly board area, layers, finish and
shipping. Open the line items and check:
- [ ] Express shipping (often the biggest part)
- [ ] Rush build time
- [ ] Extra options ticked by mistake (impedance control, special material)
- [ ] Quantity or size bigger than you meant
- [ ] Get the same quote from one or two other fabs

## Chip choice for your own board
The firmware only uses pins GP0-GP22 and GP26-GP28, plus GP25 for the LED.
That fits the **RP2350A** (QFN-60), the same chip as the Pico 2. It is easier
to solder than the 80-pin RP2350B and keeps the firmware unchanged.
Follow Raspberry Pi's "Hardware design with RP2350" guide for power parts
(inductor and capacitors), the crystal and the flash chip.
