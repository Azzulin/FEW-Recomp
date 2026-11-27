#!/usr/bin/env python3
"""Draws the window's crest (ui.h draw_crest: a gold-rimmed blue shield, an upright sword, a ring) into few.ico, the
icon of FEW Recomp Setup.exe. Our own drawing, no game art. Standard library only; the .ico is committed, so run this
only to change the icon:

    python tools/build_ui/make_icon.py tools/build_ui/few.ico
"""
import math
import struct
import sys
import zlib

SIZES = (16, 24, 32, 48, 64, 128, 256)
NAVY, GOLD, GOLD_DEEP, GOLD_LIGHT = (16, 34, 78), (222, 186, 92), (168, 124, 40), (246, 222, 146)


def shield_outline(cx, cy, size, inset=0.0, steps=48):
    """The shield as a polygon, the same shape as draw_crest (two straight top edges, sides, two curves to a point)."""
    w, h = size * 0.5, size * 0.62
    tl, tr = (cx - w + inset, cy - h * 0.86 + inset), (cx + w - inset, cy - h * 0.86 + inset)
    pts = [tl, (cx, cy - h * 0.86 + inset - size * 0.07), tr, (cx + w - inset, cy - h * 0.05)]

    def bezier(p0, p1, p2, p3):
        for i in range(1, steps + 1):
            t = i / steps
            a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
            pts.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))

    bottom = (cx, cy + h - inset)
    bezier(pts[-1], (cx + w - inset, cy + h * 0.45), (cx + w * 0.35, cy + h * 0.8), bottom)
    bezier(bottom, (cx - w * 0.35, cy + h * 0.8), (cx - w + inset, cy + h * 0.45), (cx - w + inset, cy - h * 0.05))
    return pts


def inside(poly, x, y):
    hit, j = False, len(poly) - 1
    for i in range(len(poly)):
        (xi, yi), (xj, yj) = poly[i], poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            hit = not hit
        j = i
    return hit


def render(n, ss=4):
    """RGBA rows of an n x n icon, supersampled ss x ss for smooth edges."""
    big = n * ss
    # The shield spans 1.22 x its size from its top edge to its point: this fits it with a small margin.
    cx, cy, size = big / 2, big * 0.493, big * 0.785
    outer, inner = shield_outline(cx, cy, size), shield_outline(cx, cy, size, size * 0.07)
    ring_r, ring_w, ring_y = size * 0.2, max(size * 0.035, 1.0), cy - size * 0.62 * 0.12
    bw, h = size * 0.055, size * 0.62
    tip_y, guard_y = cy - h * 0.78, cy + h * 0.18

    def color(x, y):
        if not inside(outer, x, y):
            return None
        if not inside(inner, x, y):
            return GOLD
        d = math.hypot(x - cx, y - ring_y)
        sword = (abs(x - cx) <= bw and tip_y + size * 0.1 <= y <= guard_y) or \
                (tip_y <= y < tip_y + size * 0.1 and abs(x - cx) <= bw * (y - tip_y) / (size * 0.1))
        if sword:
            return GOLD_LIGHT
        if abs(x - cx) <= size * 0.22 and guard_y <= y <= guard_y + size * 0.06:
            return GOLD
        if abs(x - cx) <= bw * 0.8 and guard_y + size * 0.06 <= y <= guard_y + size * 0.2:
            return GOLD_DEEP
        if math.hypot(x - cx, y - (guard_y + size * 0.24)) <= size * 0.05:
            return GOLD
        if abs(d - ring_r) <= ring_w / 2:
            return GOLD_DEEP
        return NAVY

    rows = []
    for py in range(n):
        row = bytearray()
        for px in range(n):
            r = g = b = a = 0
            for sy in range(ss):
                for sx in range(ss):
                    c = color(px * ss + sx + 0.5, py * ss + sy + 0.5)
                    if c:
                        r, g, b, a = r + c[0], g + c[1], b + c[2], a + 1
            row += bytes((r // a, g // a, b // a, a * 255 // (ss * ss)) if a else (0, 0, 0, 0))
        rows.append(bytes(row))
    return rows


def png(n, rows):
    raw = b"".join(b"\0" + r for r in rows)

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def main(out):
    images = [png(n, render(n, 4 if n <= 64 else 2)) for n in SIZES]
    header = struct.pack("<HHH", 0, 1, len(images))
    offset, entries = 6 + 16 * len(images), b""
    for n, data in zip(SIZES, images):
        entries += struct.pack("<BBBBHHII", n % 256, n % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    with open(out, "wb") as f:
        f.write(header + entries + b"".join(images))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "few.ico")
