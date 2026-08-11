#!/usr/bin/env python3
"""Convert the boot ROM's font.h glyph table into a woff2 webfont.

font.h packs each ASCII 32..126 glyph as 8 bytes; print.c loads them as a
little-endian uint64 and consumes bits MSB-first, row-major, 7 wide x 9
tall, advancing 6 px per character. Each ink pixel becomes a PX-unit
square contour.
"""
import re
import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

PX = 128
W, H, ADV = 7, 9, 6
FIRST, LAST = 32, 126


def parse_font_h(path):
    text = open(path).read()
    m = re.search(r"font\[\]\s*=\s*\{(.*?)\}", text, re.S)
    if not m:
        sys.exit(f"font[] not found in {path}")
    data = bytes(int(t, 16) for t in re.findall(r"0x[0-9a-fA-F]{1,2}", m.group(1)))
    need = (LAST - FIRST + 1) * 8
    if len(data) < need:
        sys.exit(f"font[] too short: {len(data)} < {need}")
    return data


def glyph_pixels(data, code):
    off = (code - FIRST) * 8
    u = int.from_bytes(data[off:off + 8], "little")
    for i in range(W * H):
        if u & (1 << (63 - i)):
            yield i % W, i // W


def build(data):
    names = ["space" if c == 32 else f"uni{c:04X}" for c in range(FIRST, LAST + 1)]
    fb = FontBuilder(H * PX, isTTF=True)
    fb.setupGlyphOrder([".notdef"] + names)
    fb.setupCharacterMap({c: n for c, n in zip(range(FIRST, LAST + 1), names)})
    glyphs, metrics = {}, {}
    pen = TTGlyphPen(None)
    glyphs[".notdef"], metrics[".notdef"] = pen.glyph(), (ADV * PX, 0)
    for c, name in zip(range(FIRST, LAST + 1), names):
        pen = TTGlyphPen(None)
        for x, y in glyph_pixels(data, c):
            x0, y0 = x * PX, (H - 1 - y) * PX
            pen.moveTo((x0, y0))
            pen.lineTo((x0 + PX, y0))
            pen.lineTo((x0 + PX, y0 + PX))
            pen.lineTo((x0, y0 + PX))
            pen.closePath()
        glyphs[name], metrics[name] = pen.glyph(), (ADV * PX, 0)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=H * PX, descent=0)
    fb.setupOS2(sTypoAscender=H * PX, sTypoDescender=0,
                usWinAscent=H * PX, usWinDescent=0)
    fb.setupNameTable({"familyName": "KIROM", "styleName": "Regular",
                       "fullName": "KIROM 7x9", "psName": "KIROM-Regular"})
    fb.setupPost()
    fb.font.flavor = "woff2"
    return fb.font


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: font2woff2.py <font.h> <out.woff2>")
    build(parse_font_h(sys.argv[1])).save(sys.argv[2])
    print("wrote", sys.argv[2])
