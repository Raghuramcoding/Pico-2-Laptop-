#!/usr/bin/env python3
"""Builds small FAT16 / FAT32 / MBR-partitioned disk images with real tools
(mkfs.vfat + mtools), then checks our FAT driver reads them correctly."""
import os, random, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "images")
os.makedirs(IMG, exist_ok=True)

def fnv(b):
    h = 2166136261
    for x in b:
        h = ((h ^ x) * 16777619) & 0xFFFFFFFF
    return h

def sh(*a, **k):
    subprocess.run(a, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **k)

def make_files():
    d = os.path.join(IMG, "src"); os.makedirs(os.path.join(d, "sub", "deeper"), exist_ok=True)
    random.seed(7)
    files = {
        "SMALL.TXT": b"hello from the sd card\n",
        "BIG.BIN": bytes(random.randrange(256) for _ in range(150000)),
        "EXACT.BIN": bytes(range(256)) * 8,                      # exactly 2048 bytes
        "sub/INNER.TXT": b"this is the inner file\n",
        "sub/deeper/X.DAT": bytes(random.randrange(256) for _ in range(5000)),
        "readme.txt": b"lower case name\n",
    }
    for n, data in files.items():
        with open(os.path.join(d, n), "wb") as f: f.write(data)
    man = os.path.join(IMG, "manifest.txt")
    with open(man, "w") as f:
        f.write("D /sub 0 0\nD /sub/deeper 0 0\n")
        for n, data in files.items():
            f.write(f"F /{n} {len(data)} {fnv(data):x}\n")
    return d, man

def populate(target, src):
    sh("mcopy", "-i", target, "-s", *[os.path.join(src, x) for x in os.listdir(src)], "::")

def main():
    src, man = make_files()
    ok = True
    cases = []
    # FAT16, superfloppy
    p = os.path.join(IMG, "fat16.img"); sh("truncate", "-s", "32M", p)
    sh("mkfs.vfat", "-F", "16", "-s", "4", p); populate(p, src); cases.append((p, 0))
    # FAT32, superfloppy, tiny clusters (stress the cluster chain code)
    p = os.path.join(IMG, "fat32.img"); sh("truncate", "-s", "64M", p)
    sh("mkfs.vfat", "-F", "32", "-s", "1", p); populate(p, src); cases.append((p, 0))
    # FAT32 inside an MBR partition starting at sector 2048
    p = os.path.join(IMG, "fat32_mbr.img"); sh("truncate", "-s", "80M", p)
    sh("mkfs.vfat", "-F", "32", "-s", "1", "--offset", "2048", p, str((80 * 1024 * 1024 // 512 - 2048) // 2))
    populate(p + "@@1048576", src)
    with open(p, "r+b") as f:
        mbr = bytearray(512)
        total = 80 * 1024 * 1024 // 512 - 2048
        mbr[446:462] = struct.pack("<B3sB3sII", 0x80, b"\x00\x00\x00", 0x0C, b"\x00\x00\x00", 2048, total)
        mbr[510:512] = b"\x55\xAA"
        f.write(mbr)
    cases.append((p, 0))
    exe = os.path.join(HERE, "test_all")
    r = subprocess.run([exe, "selftest"])
    ok &= r.returncode == 0
    for img, off in cases:
        r = subprocess.run([exe, "fat", img, man, str(off)])
        ok &= r.returncode == 0
    print("ALL HOST TESTS PASSED" if ok else "HOST TESTS FAILED")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
