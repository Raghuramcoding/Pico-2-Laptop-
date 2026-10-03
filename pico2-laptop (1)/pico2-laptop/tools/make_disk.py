#!/usr/bin/env python3
"""Make a virtual SD card image from a folder of files.
usage: make_disk.py FOLDER OUT.img [size_mb]     (needs mkfs.vfat and mcopy: dosfstools + mtools)
Use short 8.3 names (README.TXT, NOTES.TXT).  Max size is 16 MB."""
import os, subprocess, sys
if len(sys.argv) < 3:
    sys.exit(__doc__)
src, out = sys.argv[1], sys.argv[2]
mb = int(sys.argv[3]) if len(sys.argv) > 3 else 16
if mb > 16: sys.exit("max 16 MB")
subprocess.run(["truncate", "-s", f"{mb}M", out], check=True)
subprocess.run(["mkfs.vfat", "-F", "16", "-s", "4", "-n", "PICODISK", out], check=True, stdout=subprocess.DEVNULL)
subprocess.run(["mcopy", "-i", out, "-s", *[os.path.join(src, f) for f in os.listdir(src)], "::"], check=True)
print("wrote", out)
