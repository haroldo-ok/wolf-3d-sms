"""Front end screens (title, menus, intermission, ...): a native 256x192 canvas of VGA palette indices, the original
fonts and pictures, the tile maps and the ROM blob of one screen.

ROM blob of a screen (one 16 KB bank, mapped at 0x8000; all offsets from 0x8000; little endian):
  0x00 u16 ntiles       tile patterns, loaded at VRAM tile 0
  0x02 u16 tiles_off    ntiles * 32 bytes
  0x04 u16 nt_off       768 words (the name table as it is when the screen is shown)
  0x06 u8  pal[32]      palette 0 and palette 1 (CRAM bytes); palette 1 = palette 0 but for the ink slots
  0x26 u16 font_t0      tile of ASCII 32 of the 8x8 font (95 tiles), 0xFFFF: none
  0x28 u16 dig_t0       big digit glyphs (see DIGITS) 0xFFFF: none
  0x2A u8  nitems       0x2B u16 items_off    8 bytes each: col, row, w, h (cells), cursor x, y (pixels), flags, 0
  0x2D u8  nvars        0x2E u16 vars_off     7 bytes each: col, row, w, h, nvariants, u16 data_off (nvar * w * h words)
  0x30 u16 curs_off     2 frames x 6 sprite tiles (3 wide x 2 high), loaded at VRAM tile CURS_T0; 0: no cursor
  0x32 u8  bg_slot      palette slot of the background of the text cells
  0x33 u8  flags
"""
import struct
import numpy as np
import sms, scrcore, wl_gfx

W, H = 256, 192
CURS_T0 = 436                   # VRAM tile of the cursor sprite tiles (sprite pattern 180..191)
MAX_TILES = 424                 # static tiles of a screen (the bank must hold the rest)

# the original colours (WL_MENU.H)
BORD, BORD2, DEACT, BKGD, STRIPE = 0x29, 0x27, 0x2B, 0x2D, 0x2C
READ, READH, VIEW, TEXT, HILITE = 0x4A, 0x47, 0x7F, 0x17, 0x13
BLACK, WHITE = 0x00, 0x0F

DIGITS = '0123456789:% '        # big digit glyphs: 2 x 2 cells each (':' and ' ' use the left cell column only)
LABELS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ!':"


def pal8():
    import wl_assets
    return np.array(wl_assets.data()[0].pal8, np.uint8)


class Canvas:
    def __init__(self, bg, h=H, w=W):
        self.p = np.full((h, w), bg, np.uint8)

    def bar(self, x, y, w, h, c):
        self.p[max(0, y):max(0, y + h), max(0, x):max(0, x + w)] = c

    def outline(self, x, y, w, h, c1, c2):
        self.bar(x, y, w + 1, 1, c2)
        self.bar(x, y, 1, h + 1, c2)
        self.bar(x, y + h, w + 1, 1, c1)
        self.bar(x + w, y, 1, h + 1, c1)

    def window(self, x, y, w, h, bkgd=BKGD, hi=BORD2, lo=DEACT):
        self.bar(x, y, w, h, bkgd)
        self.outline(x, y, w, h, hi, lo)

    def pic(self, x, y, name_or_arr, transparent=None):
        a = wl_gfx.pic256(name_or_arr) if isinstance(name_or_arr, str) else name_or_arr
        h, w = a.shape
        ys, xs = np.mgrid[0:h, 0:w]
        ok = (ys + y >= 0) & (ys + y < self.p.shape[0]) & (xs + x >= 0) & (xs + x < self.p.shape[1])
        if transparent is not None:
            ok &= a != transparent
        self.p[(ys + y)[ok], (xs + x)[ok]] = a[ok]

    def text(self, fontn, x, y, s, col):
        """the original proportional font; returns the end x"""
        f = wl_gfx.font(fontn)
        for ch in s:
            g = f.glyph(ord(ch))
            ys, xs = np.nonzero(g)
            ok = (ys + y < self.p.shape[0]) & (xs + x < self.p.shape[1])
            self.p[(ys + y)[ok], (xs + x)[ok]] = col
            x += g.shape[1]
        return x

    def text_c(self, fontn, cx, y, s, col):
        w = text_width(fontn, s)
        return self.text(fontn, cx - w // 2, y, s, col)


def text_width(fontn, s):
    f = wl_gfx.font(fontn)
    return sum(int(f.wid[ord(c)]) for c in s)


def shrink_cols(a, k, gmin=2, gmax=6):
    """drop k columns of a (h x w) where it hurts the least"""
    if k == 0:
        return a
    drop = set(scrcore._choose(scrcore._line_costs(a), k, gmin, gmax))
    return a[:, [c for c in range(a.shape[1]) if c not in drop]]


def label_glyph(ch, pitch=12):
    """the big L_ letters shrunk to `pitch` columns (16 -> 12), picture indices; None for a space"""
    names = {'!': 'L_EXPOINTPIC', "'": 'L_APOSTROPHEPIC', ':': 'L_COLONPIC', '%': 'L_PERCENTPIC'}
    if ch == ' ':
        return None
    if ch.isdigit():
        n = 'L_NUM%sPIC' % ch
    elif ch.isalpha():
        n = 'L_%sPIC' % ch.upper()
    else:
        n = names[ch]
    a = wl_gfx.pic256(n)
    if a.shape[1] == 16:
        a = shrink_cols(a, 16 - pitch)
    return a


def label_text(cv, x, y, s, pitch=12, transparent=None):
    """text of the big font at pixel x, y; returns the end x"""
    for ch in s:
        g = label_glyph(ch, pitch)
        if g is None:
            x += pitch
            continue
        cv.pic(x, y, g, transparent=transparent)
        x += g.shape[1]
    return x


def label_width(s, pitch=12):
    w = 0
    for ch in s:
        g = label_glyph(ch, pitch)
        w += pitch if g is None else g.shape[1]
    return w


def to_tiles(a, lut):
    """index array (multiples of 8) -> list of 8x8 slot arrays (row major)"""
    h, w = a.shape
    s = lut[a]
    return [s[y:y + 8, x:x + 8].copy() for y in range(0, h, 8) for x in range(0, w, 8)]


class ScreenDef:
    """a screen: canvas + what the program needs to know about it"""

    def __init__(self, name, bg=BORD, kind='ui'):
        self.name = name
        self.cv = Canvas(bg)
        self.kind = kind
        self.items = []         # (col, row, w, h, cx, cy, flags)
        self.vars = []          # (col, row, [arrays of (h*8, w*8) indices]) variants of a rectangle
        self.font_bg = None     # VGA index of the background of the 8x8 text cells (None: no font)
        self.digits = None      # VGA index of the background of the digit cells
        self.cursor = None      # (frame1 array, frame2 array) 24x16 indices
        self.cursor_bg = None
        self.fixed = [(2, 0, 0), (2, 2, 2), (3, 3, 0), (1, 0, 0), (0, 0, 0), (3, 3, 3), (3, 0, 0)]
        self.slot_of = {BORD: 0, BORD2: 6, TEXT: 1, READH: 2, BKGD: 3, DEACT: 3, STRIPE: 3, BLACK: 4, WHITE: 5}
        self.hi = {1: (3, 3, 3), 2: (3, 3, 3)}
        self.protect = (TEXT, READH)
        self.protect_cells = ()     # (col0, row0, col1, row1) rectangles whose tiles are never merged away
        self.max_tiles = MAX_TILES
        self.fixed_photo = ()
        self.after = None       # callback(cv) after the photo/scale stage (not used by ui screens)

    def item(self, col, row, w, h, cx, cy, flags=0):
        self.items.append((col, row, w, h, cx, cy, flags))

    def variant(self, col, row, arrs):
        self.vars.append((col, row, arrs))


def _glyph8_tiles(ink, bg):
    out = []
    for c in range(32, 127):
        g = wl_gfx.cell_glyph(c)
        out.append(np.where(g, ink, bg).astype(np.uint8))
    return out


def _digit_tiles(lut, bgidx):
    """big digit glyphs -> tiles: per glyph 4 tiles (TL, TR, BL, BR)"""
    out = []
    for ch in DIGITS:
        if ch == ' ':
            a = np.full((16, 16), bgidx, np.uint8)
        else:
            n = {':': 'L_COLONPIC', '%': 'L_PERCENTPIC'}.get(ch, 'L_NUM%sPIC' % ch)
            g = wl_gfx.pic256(n)
            a = np.full((16, 16), bgidx, np.uint8)
            a[:, :g.shape[1]] = g
        t = to_tiles(a, lut)         # TL TR BL BR in row major order
        out += t
    return out


def build_ui(sd, pal):
    """quantise a ui screen and pack the blob"""
    cv = sd.cv.p
    extra, extraw = [], []
    srcs = []
    if sd.cursor is not None:
        srcs += list(sd.cursor)
    for (c, r, arrs) in sd.vars:
        srcs += list(arrs)
    for a in srcs:
        u, cnt = np.unique(a, return_counts=True)
        for v, k in zip(u, cnt):
            if int(v) not in sd.slot_of:
                extra.append(pal[v])
                extraw.append(float(k) * 0.25)
    res = scrcore.quantize_ui(cv, pal, sd.fixed, sd.slot_of, extra, extraw, sd.protect, limit=sd.max_tiles, hi=sd.hi,
                              protect_cells=sd.protect_cells)
    lut = res['lut']
    tiles = list(res['tiles'])
    nt = [list(r) for r in res['nt']]
    seen = {t.tobytes(): i for i, t in enumerate(tiles)}

    def add_tile(t, dedupe=True):
        k = t.tobytes()
        if dedupe and k in seen:
            return seen[k]
        tiles.append(t)
        seen[k] = len(tiles) - 1
        return len(tiles) - 1

    font_t0 = 0xFFFF
    if sd.font_bg is not None:
        font_t0 = len(tiles)
        for t in _glyph8_tiles(1, int(lut[sd.font_bg])):
            tiles.append(t)
    dig_t0 = 0xFFFF
    if sd.digits is not None:
        dig_t0 = len(tiles)
        for t in _digit_tiles(lut, sd.digits):
            tiles.append(t)
    var_recs, var_data = [], []
    for (c, r, arrs) in sd.vars:
        h, w = arrs[0].shape[0] // 8, arrs[0].shape[1] // 8
        ents = []
        for a in arrs:
            ts = to_tiles(a, lut)
            ents.append([add_tile(t) for t in ts])
        var_recs.append((c, r, w, h, len(arrs), ents))
        for y in range(h):
            for x in range(w):
                nt[r + y][c + x] = (ents[0][y * w + x], 0)
    assert len(tiles) <= max(MAX_TILES, sd.max_tiles), '%s: %d tiles' % (sd.name, len(tiles))
    # sprite tiles of the cursor
    curs = b''
    if sd.cursor is not None:
        for fr in sd.cursor:
            a = np.where(fr == sd.cursor_bg, 0, lut[fr]).astype(np.uint8)
            # sprite order: 3 wide x 2 high, column major (each 8 x 8)
            for cx in range(3):
                for cy in range(2):
                    curs += scrcore.planar(a[cy * 8:cy * 8 + 8, cx * 8:cx * 8 + 8])
    # blob
    hdr_len = 0x40
    tile_blob = b''.join(scrcore.planar(t) for t in tiles)
    nt_blob = b''.join(struct.pack('<H', t | fl) for row in nt for (t, fl) in row)
    item_blob = b''.join(struct.pack('<BBBBBBBB', c, r, w, h, cx, cy, fl, 0) for (c, r, w, h, cx, cy, fl) in sd.items)
    off = hdr_len
    tiles_off = off
    off += len(tile_blob)
    nt_off = off
    off += len(nt_blob)
    items_off = off
    off += len(item_blob)
    vars_off = off
    var_hdr = b''
    off += 7 * len(var_recs)
    vdata = b''
    dpos = off
    recs = b''
    for (c, r, w, h, n, ents) in var_recs:
        recs += struct.pack('<BBBBBH', c, r, w, h, n, dpos + len(vdata))
        for e in ents:
            vdata += b''.join(struct.pack('<H', i) for i in e)
    off = dpos + len(vdata)
    curs_off = off if curs else 0
    off += len(curs)
    hdr = struct.pack('<HHH', len(tiles), tiles_off, nt_off) + res['pal'][0] + res['pal'][1]
    hdr += struct.pack('<HH', font_t0, dig_t0)
    hdr += struct.pack('<BHBHH', len(sd.items), items_off, len(var_recs), vars_off, curs_off)
    hdr += struct.pack('<BB', int(lut[sd.font_bg if sd.font_bg is not None else BORD]), 0)
    hdr += b'\0' * (hdr_len - len(hdr))
    blob = hdr + tile_blob + nt_blob + item_blob + recs + vdata + curs
    assert len(blob) <= 16384, '%s: blob %d bytes' % (sd.name, len(blob))
    prev = scrcore.render_screen(tiles, nt, res['pal_rgb'])
    return blob, prev


def build_photo(name, idx, pal, limit=440, fixed=(), protect_cells=()):
    r = scrcore.quantize_photo(idx, pal, limit, fixed=fixed, protect_cells=protect_cells)
    tiles, nt = r['tiles'], r['nt']
    hdr_len = 0x40
    tile_blob = b''.join(scrcore.planar(t) for t in tiles)
    nt_blob = b''.join(struct.pack('<H', t | fl) for row in nt for (t, fl) in row)
    hdr = struct.pack('<HHH', len(tiles), hdr_len, hdr_len + len(tile_blob)) + r['pal'][0] + r['pal'][1]
    hdr += struct.pack('<HH', 0xFFFF, 0xFFFF)
    hdr += struct.pack('<BHBHH', 0, 0, 0, 0, 0)
    hdr += struct.pack('<BB', 0, 0)
    hdr += b'\0' * (hdr_len - len(hdr))
    blob = hdr + tile_blob + nt_blob
    assert len(blob) <= 16384, '%s: blob %d bytes (%d tiles)' % (name, len(blob), len(tiles))
    prev = scrcore.render_screen(tiles, nt, r['pal_rgb'])
    return blob, prev
