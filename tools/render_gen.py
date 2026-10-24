"""Renderer data for the Wolfenstein 3D SMS port (the scalers and the data
layout derive from the Catacomb Abyss SMS generator).

View: 224x80 pixels = 28x10 tiles, 56 rays of 4 pixels.

Projection: Wolfenstein 3D's own (WL_DRAW.C CalcProjection).  The eye is
FOCALLENGTH + MINDIST = 0x5800 + 0x5800 = 0.6875 tile from a view plane that
is VIEWGLOBAL = 1 tile wide, so

    scale = halfview * facedist / (VIEWGLOBAL / 2) = halfview * 1.375

which is both the horizontal focal length in pixels (horizontal FOV = 2 atan
(0.5 / 0.6875) = 72 degrees) and the height of a wall at distance 1 in the
original's pixels.  Those are 1.2 times as tall as wide (320x200 on a 4:3
screen), so on the SMS's square pixels a wall is 1.2 * scale / dist pixels
tall, and a sprite is 1.2 times as high as it is wide (as are the original's).

Outputs (raw bank images, generated machine code where needed):
  RD bank (mapped at slot 1, 0x4000): plane tables, trig tables, column
     tables, sprite start tables, hand overlay, wall picture tables
  scaler banks (slot 1): compiled wall scalers, position independent
  texture banks (slot 2): 64x64 walls, byte per texel (c * 0x11),
     column major, 4 per bank
  shape banks (slot 2): scaled shape column lists
"""
import math, re, os
import numpy as np

VIEW_W, VIEW_H = 192, 96
NRAYS = 48
CY = 48
MAXS = 110
FOCALLENGTH, MINDIST, VIEWGLOBAL = 0x5800, 0x5800, 0x10000
KH = (VIEW_W / 2.0) * (FOCALLENGTH + MINDIST) / (VIEWGLOBAL / 2.0)   # 154.0 px
PIXEL_ASPECT = 1.2
WALL_K = KH * PIXEL_ASPECT                # full wall height at distance 1 (px)
KV_HALF = WALL_K / 2.0                    # half height at distance 1 tile
HSCALE = KH / WALL_K                      # width / height scale of a sprite
HALF_FOV8 = int(math.ceil(math.atan((VIEW_W / 2.0) / KH) / (2 * math.pi) * 256))   # in 1/256 turns
FINE = 3600


# --------------------------------------------------------------- tables
def texv(c):
    """texel byte: colour c (0..15, or c*0x11) -> 0x80 + 4c.  As the high
    byte it selects the plane table page of a left pixel, as the low byte the
    entry of a right pixel (see plane_bank)."""
    return 0x80 + 4 * (c & 15)


def plane_bank():
    """slot 2 table: [(0x80+4L+b) << 8 | (0x80+4R)] = bitplane b of LLLLRRRR"""
    out = bytearray(0x4000)
    for L in range(16):
        for b in range(4):
            for R in range(16):
                v = (0xF0 if (L >> b) & 1 else 0) | (0x0F if (R >> b) & 1 else 0)
                out[(4 * L + b) * 256 + 0x80 + 4 * R] = v
    return bytes(out)


def plane_tables():
    out = bytearray()
    for b in range(4):
        for idx in range(256):
            L, R = idx >> 4, idx & 15
            v = (0xF0 if (L >> b) & 1 else 0) | (0x0F if (R >> b) & 1 else 0)
            out.append(v)
    return bytes(out)


def column_tables():
    fine, colk, colmin = [], [], []
    for c in range(NRAYS):
        t = ((4 * c + 2) - VIEW_W / 2.0) / KH
        a = -math.atan(t)
        fine.append(int(round(a * FINE / (2 * math.pi))))
        cs = math.cos(a)
        colk.append(min(65535, int(round(KV_HALF * 256 / cs))))
        colmin.append(int(math.ceil(256 / cs)))
    return fine, colk, colmin


def trig_quarter():
    """for fine angle 0..900: cos (0.16, 65535 max), 256/cos (8.8, saturated)"""
    cosq, invq = [], []
    for a in range(901):
        c = math.cos(a * 2 * math.pi / FINE)
        cosq.append(min(65535, int(round(c * 65535))))
        invq.append(min(65535, int(256.0 / c)) if c > 1e-9 else 65535)
    return cosq, invq


def row_range(s):
    return max(0, CY - s), min(VIEW_H, CY + s)


def texel_of_row(s, y):
    return ((y - (CY - s)) * 64) // (2 * s)


def sprite_starts():
    out = bytearray()
    for s in range(MAXS + 1):
        ss = max(s, 1)
        for src in range(65):
            r = (CY - ss) + (src * 2 * ss) // 64
            out.append(max(0, min(VIEW_H, r)))
    return bytes(out)


def sprite_stepinv():
    """source columns per screen pixel (8.8): the sprite is HSCALE as wide as it is high"""
    return [0] + [min(65535, int(32 * 256 / (s * HSCALE))) for s in range(1, MAXS + 1)]

# --------------------------------------------------------------- scalers
def advance_l(k):
    """add k to L (texture column pointer, never crosses its 64 byte column)"""
    if k <= 0:
        return b''
    if k <= 3:
        return b'\x2C' * k                      # inc l
    return bytes([0x7D, 0xC6, k, 0x6F])          # ld a,l ; add a,k ; ld l,a


def scaler_code(s):
    """HL = texture column (64 byte aligned), DE = colbuf page + y0"""
    y0, y1 = row_range(s)
    code = bytearray()
    last, cur = None, 0
    for y in range(y0, y1):
        r = texel_of_row(s, y)
        if r != last:
            code += advance_l(r - cur)
            cur = r
            code += b'\x7E'                     # ld a,(hl)
            last = r
        code += b'\x12\x1C'                     # ld (de),a ; inc e
    code.append(0xC9)
    return bytes(code)


def sprite_starts_s(s):
    ss = max(s, 1)
    return [max(0, min(VIEW_H, (CY - ss) + (src * 2 * ss) // 64)) for src in range(65)]


def sprite_scaler(s, base):
    """256 aligned block at `base`: [0..63] first row of texel r,
    [64..127] entry lo, [128..191] entry hi, code from 192.
    Entry for texel r: HL = pixels, DE = colbuf page + starts[r], B = count.
    Each texel block: ld a,(hl) ; inc hl ; (ld (de),a ; inc e) * rows ; dec b ; ret z"""
    st = sprite_starts_s(s)
    code = bytearray()
    entries = []
    for r in range(64):
        entries.append(base + 192 + len(code))
        k = st[r + 1] - st[r]
        if k:
            code += b'\x7E\x23' + b'\x12\x1C' * k
        else:
            code += b'\x23'
        code += b'\x05\xC8'
    code.append(0xC9)
    blk = bytearray(st[:64]) + bytes(e & 0xFF for e in entries) + bytes(e >> 8 for e in entries)
    return bytes(blk) + bytes(code)


def build_sprite_scalers():
    banks = [bytearray()]
    table = [(0, 0x4000)]
    banks[-1] += sprite_scaler(0, 0x4000)
    for s in range(1, MAXS + 1):
        b = banks[-1]
        b += bytes((-len(b)) % 256)
        c = sprite_scaler(s, 0x4000 + len(b))
        if len(b) + len(c) > 0x4000:
            banks.append(bytearray())
            b = banks[-1]
            c = sprite_scaler(s, 0x4000)
        table.append((len(banks) - 1, 0x4000 + len(b)))
        b += c
    return banks, table


def build_scalers():
    """-> list of bank images, table of (bank_index, addr) per s"""
    banks = [bytearray()]
    table = [(0, 0x4000)]
    banks[-1] += b'\xC9'       # s = 0: just ret
    for s in range(1, MAXS + 1):
        c = scaler_code(s)
        if len(banks[-1]) + len(c) > 0x4000:
            banks.append(bytearray())
        table.append((len(banks) - 1, 0x4000 + len(banks[-1])))
        banks[-1] += c
    return banks, table


def u16le(vals):
    out = bytearray()
    for v in vals:
        v &= 0xFFFF
        out += bytes([v & 0xFF, v >> 8])
    return bytes(out)


