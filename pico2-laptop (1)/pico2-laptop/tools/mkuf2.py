#!/usr/bin/env python3
"""Make a UF2 file for the RP2350 (ARM, secure) from raw .bin pieces.

usage: mkuf2.py -o out.uf2 0x10000000:bios.bin [0x10040000:os.bin ...]
Copy the .uf2 onto the RPI-RP2 drive that shows up when you hold BOOTSEL
while plugging the Pico 2 into USB.
"""
import struct, sys

FAMILY_RP2350_ARM_S = 0xE48BFF59
MAGIC0, MAGIC1, MAGIC_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
FLAG_FAMILY = 0x00002000

def blocks_for(addr, data):
    for off in range(0, len(data), 256):
        chunk = data[off:off + 256]
        yield addr + off, chunk.ljust(256, b"\xff")

def main(argv):
    if len(argv) < 4 or argv[1] != "-o":
        print(__doc__); return 1
    out = argv[2]
    pieces = []
    for spec in argv[3:]:
        a, f = spec.split(":", 1)
        with open(f, "rb") as fh:
            pieces.append((int(a, 0), fh.read()))
    blocks = [b for a, d in pieces for b in blocks_for(a, d)]
    total = len(blocks)
    with open(out, "wb") as fh:
        for n, (addr, payload) in enumerate(blocks):
            hdr = struct.pack("<8I", MAGIC0, MAGIC1, FLAG_FAMILY, addr, 256, n, total, FAMILY_RP2350_ARM_S)
            fh.write(hdr + payload.ljust(476, b"\x00") + struct.pack("<I", MAGIC_END))
    print(f"{out}: {total} blocks, {total * 512} bytes")
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
