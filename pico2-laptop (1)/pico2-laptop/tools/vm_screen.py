"""Turns the VM's "LCD" serial stream back into a picture of the 320x240 screen.

Stream format (one line per redrawn text row):  R<row>,<cursor col or -1>,<40 characters>
"""
import struct, threading, zlib
from font8x8 import FONT

COLS, ROWS = 40, 30
FG = (0x80, 0xFF, 0x80)       # same colour the OS uses (RGB565 0x87F0)
BG = (0, 0, 0)


class Screen:
    def __init__(self):
        self.rows = [" " * COLS for _ in range(ROWS)]
        self.cursor = (-1, -1)
        self.buf = b""
        self.lock = threading.Lock()
        self.version = 0

    def feed(self, data: bytes):
        with self.lock:
            self.buf += data
            while b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                self._line(line.decode("latin-1").rstrip("\r"))

    def _line(self, line):
        if not line.startswith("R"):
            return
        try:
            r, c, text = line[1:].split(",", 2)
            r, c = int(r), int(c)
        except ValueError:
            return
        if not 0 <= r < ROWS:
            return
        self.rows[r] = text[:COLS].ljust(COLS)
        if c >= 0:
            self.cursor = (r, c)
        elif self.cursor[0] == r:
            self.cursor = (-1, -1)
        self.version += 1

    def render(self, scale=1):
        """Returns (width, height, rgb bytes)."""
        with self.lock:
            rows = list(self.rows)
            cur = self.cursor
        w, h = COLS * 8, ROWS * 8
        img = bytearray(w * h * 3)
        for r in range(ROWS):
            for c in range(COLS):
                ch = ord(rows[r][c])
                glyph = FONT[ch - 32] if 32 <= ch <= 126 else FONT[ord("?") - 32]
                inv = (r, c) == cur
                for y in range(8):
                    bits = glyph[y]
                    base = ((r * 8 + y) * w + c * 8) * 3
                    for x in range(8):
                        on = ((bits >> x) & 1) ^ inv
                        img[base + x * 3: base + x * 3 + 3] = bytes(FG if on else BG)
        if scale > 1:
            big = bytearray()
            for y in range(h):
                row = bytes(img[y * w * 3:(y + 1) * w * 3])
                wide = b"".join(row[i:i + 3] * scale for i in range(0, len(row), 3))
                big += wide * scale
            return w * scale, h * scale, bytes(big)
        return w, h, bytes(img)

    def write_png(self, path, scale=2):
        w, h, rgb = self.render(scale)
        raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
        def chunk(tag, data):
            body = tag + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) \
              + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
        with open(path, "wb") as f:
            f.write(png)
