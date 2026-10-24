"""Pictures, fonts and the 8x8 text font of the original VGAGRAPH."""
import struct
import numpy as np
import wolfdata, wl_names

_cache = {}


def _d():
    import wl_assets
    return wl_assets.data()[0]


def pic_table():
    if 'pt' not in _cache:
        _cache['pt'] = _d().vga_chunk(0)
    return _cache['pt']


def pic_index(name):
    if 'names' not in _cache:
        _cache['names'] = dict(wl_names.gfx_names())
    return _cache['names'][name]


def pic256(name):
    """picture as an h x w array of VGA palette indices"""
    d = _d()
    c = pic_index(name)
    w, h = struct.unpack_from('<HH', pic_table(), (c - 3) * 4)
    raw = d.vga_chunk(c)
    assert len(raw) == w * h, (name, w, h, len(raw))
    return np.array(wolfdata.deplane(raw, w, h), dtype=np.uint8)


def pic_rgb(name):
    d = _d()
    p = pic256(name)
    pal = np.array(d.pal8, dtype=np.uint8)
    return pal[p]


class Font:
    """proportional font: glyph(c) -> (w, height, bool array)"""
    def __init__(self, chunk):
        f = _d().vga_chunk(chunk)
        self.height = struct.unpack_from('<H', f, 0)[0]
        self.loc = struct.unpack_from('<256H', f, 2)
        self.wid = f[2 + 512:2 + 512 + 256]
        self.f = f

    def glyph(self, c):
        w = self.wid[c]
        o = self.loc[c]
        a = np.zeros((self.height, w), bool)
        for y in range(self.height):
            for x in range(w):
                a[y, x] = self.f[o + y * w + x] != 0
        return a


def font(n):
    if ('font', n) not in _cache:
        _cache[('font', n)] = Font(1 + n)
    return _cache[('font', n)]


def cell_glyph(c):
    """8x8 glyph of the small font for a fixed width cell: rows 1..8 of the
    font; glyphs wider than 8 lose columns that match a neighbour"""
    g = font(0).glyph(c)[1:9]
    w = g.shape[1]
    if w == 0:
        return np.zeros((8, 8), bool)
    # trim trailing empty columns, keep one spacing column when it fits
    cols = [x for x in range(w) if g[:, x].any()]
    last = max(cols) if cols else -1
    g = g[:, :last + 1]
    while g.shape[1] > 8:
        best = None
        for x in range(1, g.shape[1] - 1):
            cost = min(int((g[:, x] != g[:, x - 1]).sum()), int((g[:, x] != g[:, x + 1]).sum()))
            if best is None or cost < best[0]:
                best = (cost, x)
        g = np.delete(g, best[1], axis=1)
    out = np.zeros((8, 8), bool)
    out[:, :g.shape[1]] = g
    return out


def text_bitmap(text, fontn=0, spacing=0):
    """render a string with the proportional font -> bool array"""
    f = font(fontn)
    cols = []
    for ch in text:
        g = f.glyph(ord(ch))
        cols.append(g)
        if spacing:
            cols.append(np.zeros((f.height, spacing), bool))
    if not cols:
        return np.zeros((f.height, 0), bool)
    return np.concatenate(cols, axis=1)
