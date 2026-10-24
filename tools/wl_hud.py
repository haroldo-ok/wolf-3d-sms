"""HUD (status panel), face pictures, weapon icons and the weapon sprites.

Palette 1 (CRAM 16..31: the sprite palette, also used by the HUD cells of the
background) holds the HUD and weapon colours.  Entry 0 is the panel colour
(and transparent for the sprites), 1 the label white.

HUD layout (cells; screen rows 12..23 = HUD rows 0..11), everything at the original's 1:1 scale:
  top bar    rows 1..5   : HEALTH | SCORE | AMMO
  bottom bar rows 6..11  : FLOOR | weapon icon | FACE | keys | LIVES   (the face is the 24x32 picture,
                           centred on a 32x32 canvas, at the bottom centre of the screen)
"""
import collections
import numpy as np
from PIL import Image
import sms, wl_gfx, wl_assets, render_gen as rg

VIEW_TILES = (rg.VIEW_W // 8) * (rg.VIEW_H // 8)
HUD_T0 = VIEW_TILES + 3                     # after the view's own tiles and the three flat tiles

HUD_ROW0 = 12
# the face: 4x4 cells = a 32x32 canvas holding the 24x32 picture (4 pixels of panel on each side)
FACE_C0, FACE_R0, FACE_W, FACE_H = 14, 7, 4, 4
FACE_PIC_X = 4
ICON_C0, ICON_R0, ICON_W, ICON_H = 7, 8, 6, 3
# numbers: (col, row of the top half, digits)
NUMS = {'HEALTH': (4, 3, 3), 'SCORE': (13, 3, 6), 'AMMO': (25, 3, 2), 'FLOOR': (3, 9, 2), 'LIVES': (27, 9, 1)}
PERCENT = (7, 3)
KEYS = [(20, 9), (23, 9)]                   # gold, silver (col, row), 1x2 cells
# labels: (text, x, y) in pixels of the panel, centred over their number
LABELS = [('HEALTH', 24, 16), ('SCORE', 108, 16), ('AMMO', 192, 16), ('FLOOR', 12, 63), ('LIVES', 204, 63)]
BOXES = [(1, 1, 10, 5), (11, 1, 20, 5), (21, 1, 30, 5),
         (1, 6, 6, 11), (7, 6, 12, 11), (13, 6, 18, 11), (19, 6, 24, 11), (25, 6, 30, 11)]
FACES = ['FACE%d%sPIC' % (i, l) for i in range(1, 8) for l in 'ABC'] + ['FACE8APIC', 'GOTGATLINGPIC']
ICONS = ['KNIFEPIC', 'GUNPIC', 'MACHINEGUNPIC', 'GATLINGGUNPIC']
WEAPON_FRAMES = list(range(416, 436))
# The weapon shapes (64x64, art in rows 26..63) are shown as 8x8 hardware sprites magnified 2x:
# 1 texel = 2x2 pixels, like the original's 2.4x scaling.  The canvas is the bottom 40 rows
# = 5 rows of sprites = the 80 pixel view height, 8 columns = 128 pixels.
WEAPON_Y0 = 24
WEAPON_ROWS = 5
WEAPON_SLOT = 32                            # tiles reserved per frame in ROM
WEAPON_PER_BANK = 16384 // (WEAPON_SLOT * 32)
WEAPON_MAXTILES = 28                        # VRAM budget
_cache = {}


def lab_map(rgb_img, pal_rgb, allow0=True):
    """rgb image (h,w,3) -> palette indices (nearest in Lab)"""
    h, w, _ = rgb_img.shape
    flat = rgb_img.reshape(-1, 3)
    uniq, inv = np.unique(flat, axis=0, return_inverse=True)
    ulab = sms.rgb_to_lab(uniq.astype(np.float64))
    plab = sms.rgb_to_lab(np.asarray(pal_rgb, dtype=np.float64))
    if not allow0:
        plab = plab.copy()
        plab[0] = 1e6
    dist = ((ulab[:, None, :] - plab[None, :, :]) ** 2).sum(axis=2)
    idx = dist.argmin(axis=1).astype(np.uint8)
    return idx[inv.reshape(-1)].reshape(h, w)


def weapon_canvas(i):
    """sprite i -> rgba (40, 64): the bottom 40 rows of the shape at its own resolution"""
    d, pal, m = wl_assets.data()
    sp = d.sprite(i)
    cv = np.zeros((64 - WEAPON_Y0, 64, 4), np.uint8)
    for x in range(64):
        for y in range(64):
            v = sp[x][y]
            if v is None:
                continue
            assert y >= WEAPON_Y0, 'weapon shape %d has pixels above the canvas' % i
            cv[y - WEAPON_Y0, x, :3] = d.pal8[v]
            cv[y - WEAPON_Y0, x, 3] = 255
    return cv


def build_palette():
    if 'pal' in _cache:
        return _cache['pal']
    d, _, _ = wl_assets.data()
    weights = collections.Counter()

    def add(rgb, w):
        flat = rgb.reshape(-1, 3)
        u, c = np.unique(flat, axis=0, return_counts=True)
        for col, n in zip(u, c):
            weights[tuple(int(v) for v in col)] += w * n
    for n in FACES:
        add(wl_gfx.pic_rgb(n), 3.0)
    for n in ICONS + ['NOKEYPIC', 'GOLDKEYPIC', 'SILVERKEYPIC'] + ['N_%dPIC' % i for i in range(10)]:
        add(wl_gfx.pic_rgb(n), 1.0)
    add(wl_gfx.pic_rgb('STATUSBARPIC'), 0.15)
    for i in WEAPON_FRAMES:
        cv = weapon_canvas(i)
        a = cv[:, :, 3] > 128
        add(cv[:, :, :3][a], 2.0)
    cols = np.array(list(weights.keys()), dtype=np.float64)
    w = np.array(list(weights.values()), dtype=np.float64)
    # the panel colour: the most common colour of the original status bar
    sb = wl_gfx.pic_rgb('STATUSBARPIC').reshape(-1, 3)
    u, c = np.unique(sb, axis=0, return_counts=True)
    bg = sms.snap(u[c.argmax()])
    fixed = [bg, (3, 3, 3), (0, 0, 1), (1, 2, 3)]
    fixed = list(dict.fromkeys(fixed))
    pal, err = sms.choose_palette(cols, w, 16, fixed)
    _cache['pal'] = pal
    print('hud palette', pal, 'err %.1f' % err)
    return pal


def pal_rgbs():
    return sms.pal_rgb(build_palette())


def pic_idx(name, allow0=True):
    return lab_map(wl_gfx.pic_rgb(name), pal_rgbs(), allow0)


def tile_bytes(img8):
    return sms.tile_to_planar(img8)


def tiles_of(img, c0=0, r0=0, cw=None, ch=None):
    """image (h,w) of indices -> list of tile byte strings, row major cells"""
    h, w = img.shape
    out = []
    for r in range(h // 8):
        for c in range(w // 8):
            out.append(tile_bytes(img[r * 8:r * 8 + 8, c * 8:c * 8 + 8]))
    return out


def compose_panel():
    """the static panel bitmap (96 x 256) of palette indices"""
    img = np.zeros((96, 256), np.uint8)
    LIGHT, DARK, WHITE = 3, 2, 1
    pal = build_palette()
    # index of the bevel colours in the palette (fixed ones come first, deduplicated)
    for (c0, r0, c1, r1) in BOXES:
        x0, y0, x1, y1 = c0 * 8, r0 * 8, c1 * 8 + 7, r1 * 8 + 7
        img[y0, x0:x1 + 1] = LIGHT
        img[y0:y1 + 1, x0] = LIGHT
        img[y1, x0 + 1:x1 + 1] = DARK
        img[y0 + 1:y1 + 1, x1] = DARK
    # outer frame
    img[0, :] = LIGHT
    img[95, :] = DARK
    img[:, 0] = LIGHT
    img[:, 255] = DARK
    for text, x0, y in LABELS:
        for i, ch in enumerate(text):
            g = wl_gfx.cell_glyph(ord(ch))
            x = x0 + i * 8
            blk = img[y:y + 8, x:x + 8]
            blk[g] = WHITE
    return img


def digit_tiles():
    """11 glyphs (0..9, blank), each two tiles (top, bottom), and '%'"""
    out = []
    for i in range(10):
        g = pic_idx('N_%dPIC' % i)
        out += [tile_bytes(g[:8]), tile_bytes(g[8:])]
    g = pic_idx('N_BLANKPIC')
    out += [tile_bytes(g[:8]), tile_bytes(g[8:])]
    # '%' from the 8x13 big font glyph, centred into 8x16 (white on panel)
    f = wl_gfx.font(1).glyph(ord('%'))
    pc = np.zeros((16, 8), np.uint8)
    h, w = f.shape
    for y in range(min(h, 16)):
        for x in range(min(w, 8)):
            if f[y, x]:
                pc[y + 1, x] = 1
    # squeeze to 8 columns: the glyph is 12 wide: take every 3rd of 2 columns (simple column merge)
    wide = np.zeros((16, 12), np.uint8)
    for y in range(min(h, 15)):
        for x in range(min(w, 12)):
            if f[y, x]:
                wide[y + 1, x] = 1
    keep = [0, 1, 3, 4, 6, 8, 9, 11]
    pc = wide[:, keep]
    out += [tile_bytes(pc[:8]), tile_bytes(pc[8:])]
    return out


def face_tiles(name):
    """the 24x32 face at its own scale on a 32x32 canvas (index 0: panel colour on both sides)"""
    g = pic_idx(name)
    assert g.shape == (32, 24)
    cv = np.zeros((FACE_H * 8, FACE_W * 8), np.uint8)
    cv[:, FACE_PIC_X:FACE_PIC_X + 24] = g
    return tiles_of(cv)


def icon_tiles(name):
    """the weapon picture over the panel: its background pixels (index 0) keep the box edges"""
    g = pic_idx(name)
    panel = compose_panel()[ICON_R0 * 8:(ICON_R0 + ICON_H) * 8, ICON_C0 * 8:(ICON_C0 + ICON_W) * 8]
    assert g.shape == panel.shape, (g.shape, panel.shape)
    return tiles_of(np.where(g != 0, g, panel).astype(np.uint8))


def weapon_data():
    """per frame: the non-empty 8x8 blocks of the canvas (8 columns x 5 rows) as tiles in
    row major order, padded to WEAPON_SLOT tiles, and the used mask (one byte per row,
    bit = column)"""
    pr = pal_rgbs()
    frames = []
    for i in WEAPON_FRAMES:
        cv = weapon_canvas(i)
        a = cv[:, :, 3] > 128
        idx = lab_map(cv[:, :, :3], pr, allow0=False)
        idx[~a] = 0
        tiles = []
        mask = [0] * WEAPON_ROWS
        for r in range(WEAPON_ROWS):
            for c in range(8):
                blk = idx[r * 8:r * 8 + 8, c * 8:c * 8 + 8]
                if blk.any():
                    mask[r] |= 1 << c
                    tiles.append(tile_bytes(blk))
        assert len(tiles) <= WEAPON_MAXTILES, 'weapon frame %d uses %d tiles' % (i, len(tiles))
        while len(tiles) < WEAPON_SLOT:
            tiles.append(bytes(32))
        frames.append((b''.join(tiles), mask))
    return frames


def build(pk, H, C, d, cached):
    pal = build_palette()
    H.append('extern const u8 hud_pal[16];')
    C.append('const u8 hud_pal[16] = {%s};' % ','.join(str(b) for b in sms.pal_bytes(pal)))
    panel = compose_panel()
    digits = digit_tiles()
    # tile layout (offsets from HUD_T0)
    T_DIG = 0                                   # 22 digit tiles + 2 '%' tiles
    T_STATIC = len(digits)
    # static cells
    static = []                                 # tile byte strings
    smap = {}
    cells = [[0] * 32 for _ in range(12)]
    dyn = {}
    for name, (c, r, n) in NUMS.items():
        for i in range(n):
            dyn[(c + i, r)] = ('d', 0)
            dyn[(c + i, r + 1)] = ('d', 1)
    dyn[(PERCENT[0], PERCENT[1])] = ('p', 0)
    dyn[(PERCENT[0], PERCENT[1] + 1)] = ('p', 1)
    keyc = {}
    for k, (c, r) in enumerate(KEYS):
        keyc[(c, r)] = (k, 0)
        keyc[(c, r + 1)] = (k, 1)
    for r in range(12):
        for c in range(32):
            if FACE_C0 <= c < FACE_C0 + FACE_W and FACE_R0 <= r < FACE_R0 + FACE_H:
                continue
            if ICON_C0 <= c < ICON_C0 + ICON_W and ICON_R0 <= r < ICON_R0 + ICON_H:
                continue
            if (c, r) in dyn or (c, r) in keyc:
                continue
            t = tile_bytes(panel[r * 8:r * 8 + 8, c * 8:c * 8 + 8])
            if t not in smap:
                smap[t] = len(static)
                static.append(t)
            cells[r][c] = T_STATIC + smap[t]
    nstat = len(static)
    # keys: none / gold / silver, 2 tiles each
    key_tiles = []
    for nm in ('NOKEYPIC', 'GOLDKEYPIC', 'SILVERKEYPIC'):
        g = pic_idx(nm)
        key_tiles += [tile_bytes(g[:8]), tile_bytes(g[8:])]
    T_KEYS = T_STATIC + nstat
    T_FACE = T_KEYS + 6
    T_ICON = T_FACE + FACE_W * FACE_H
    T_END = T_ICON + ICON_W * ICON_H
    # nametable cells for dynamic things
    for (c, r), (kind, half) in dyn.items():
        cells[r][c] = (T_DIG + 2 * 10 + half) if kind == 'd' else (T_DIG + 22 + half)
    for (c, r), (k, half) in keyc.items():
        cells[r][c] = T_KEYS + half              # empty slot icon (none)
    for r in range(FACE_H):
        for c in range(FACE_W):
            cells[FACE_R0 + r][FACE_C0 + c] = T_FACE + r * FACE_W + c
    for r in range(ICON_H):
        for c in range(ICON_W):
            cells[ICON_R0 + r][ICON_C0 + c] = T_ICON + r * ICON_W + c
    stat_blob = b''.join(digits) + b''.join(static) + b''.join(key_tiles)
    nt = bytearray()
    for r in range(12):
        for c in range(32):
            nt += bytes([cells[r][c] & 0xFF, 0x08 | (cells[r][c] >> 8)])   # palette 1 (bit 11)... tile base added by the loader
    b, a = pk.add('hud_static', stat_blob)
    H.append('#define HUD_STATIC_BANK %d\n#define HUD_STATIC_ADDR 0x%04X\n#define HUD_STATIC_TILES %d' % (b, a, len(stat_blob) // 32))
    b, a = pk.add('hud_nt', bytes(nt))
    H.append('#define HUD_NT_BANK %d\n#define HUD_NT_ADDR 0x%04X' % (b, a))
    ftiles = b''.join(b''.join(face_tiles(n)) for n in FACES)
    faces_per_bank = 16384 // (FACE_W * FACE_H * 32)
    fbanks = pk.add_split('hud_faces', ftiles, FACE_W * FACE_H * 32 * faces_per_bank)
    H.append('#define HUD_FACES_BANK0 %d\n#define HUD_FACES_PER_BANK %d' % (fbanks[0], faces_per_bank))
    itiles = b''.join(b''.join(icon_tiles(n)) for n in ICONS)
    b, a = pk.add('hud_icons', itiles)
    H.append('#define HUD_ICONS_BANK %d\n#define HUD_ICONS_ADDR 0x%04X' % (b, a))
    H.append('#define HUD_ROW0 %d' % HUD_ROW0)
    H.append('#define HUD_T0 %d' % HUD_T0)
    H.append('#define WEAPON_T0 %d' % ((HUD_T0 + T_END + 1) & ~1))
    H.append('#define HUD_T_DIG %d\n#define HUD_T_KEYS %d\n#define HUD_T_FACE %d\n#define HUD_T_ICON %d\n#define HUD_T_END %d' % (T_DIG, T_KEYS, T_FACE, T_ICON, T_END))
    for name, (c, r, n) in NUMS.items():
        H.append('#define HUD_%s_C %d\n#define HUD_%s_R %d\n#define HUD_%s_N %d' % (name, c, name, r, name, n))
    for k, nm in enumerate(('GOLD', 'SILVER')):
        H.append('#define HUD_KEY%s_C %d\n#define HUD_KEY%s_R %d' % (nm, KEYS[k][0], nm, KEYS[k][1]))
    H.append('#define HUD_FACE_C %d\n#define HUD_FACE_R %d\n#define HUD_FACE_W %d\n#define HUD_FACE_H %d' % (FACE_C0, FACE_R0, FACE_W, FACE_H))
    H.append('#define HUD_ICON_C %d\n#define HUD_ICON_R %d\n#define HUD_ICON_W %d\n#define HUD_ICON_H %d' % (ICON_C0, ICON_R0, ICON_W, ICON_H))
    # weapon sprites
    wd = weapon_data()
    blob = b''.join(t for t, m in wd)
    banks = pk.add_split('weapon_frames', blob, WEAPON_SLOT * 32 * WEAPON_PER_BANK)
    H.append('#define WEAPON_BANK0 %d' % banks[0])
    H.append('#define WEAPON_PER_BANK %d' % WEAPON_PER_BANK)
    H.append('#define WEAPON_SLOT %d' % WEAPON_SLOT)
    wtiles = max(sum(bin(b).count('1') for b in m) for t, m in wd)
    assert ((HUD_T0 + T_END + 1) & ~1) + wtiles <= 448, 'VRAM tiles overflow into the nametable'
    H.append('extern const u8 weapon_mask[20][5];')
    C.append('const u8 weapon_mask[20][5] = {%s};' % ','.join('{%s}' % ','.join('0x%02X' % b for b in m) for t, m in wd))
    print('hud: static tiles %d (%d unique panel), end tile %d, faces %d bytes, weapon banks %d' %
          (len(stat_blob) // 32, nstat, HUD_T0 + T_END, len(ftiles), len(banks)))
