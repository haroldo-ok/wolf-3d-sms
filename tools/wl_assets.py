"""Wolfenstein 3D -> SMS: wall textures, world sprites and the renderer data
banks (view palette: wl_view.py; scalers and tables: render_gen.py)."""
import os
import numpy as np
import wolfdata, wl_view, sms, render_gen as rg
from render_gen import texv, u16le

HERE = os.path.dirname(os.path.abspath(__file__))
import paths
DATADIR = paths.DATA
SRCDIR = paths.SRC

RNDTABLE = [0, 8, 109, 220, 222, 241, 149, 107, 75, 248, 254, 140, 16, 66, 74, 21, 211, 47, 80, 242, 154, 27, 205, 128, 161, 89, 77, 36, 95, 110, 85, 48, 212, 140, 211, 249, 22, 79, 200, 50, 28, 188, 52, 140, 202, 120, 68, 145, 62, 70, 184, 190, 91, 197, 152, 224, 149, 104, 25, 178, 252, 182, 202, 182, 141, 197, 4, 81, 181, 242, 145, 42, 39, 227, 156, 198, 225, 193, 219, 93, 122, 175, 249, 0, 175, 143, 70, 239, 46, 246, 163, 53, 163, 109, 168, 135, 2, 235, 25, 92, 20, 145, 138, 77, 69, 166, 78, 176, 173, 212, 166, 113, 94, 161, 41, 50, 239, 49, 111, 164, 70, 60, 2, 37, 171, 75, 136, 156, 11, 56, 42, 146, 138, 229, 73, 146, 77, 61, 98, 196, 135, 106, 63, 197, 195, 86, 96, 203, 113, 101, 170, 247, 181, 113, 80, 250, 108, 7, 255, 237, 129, 226, 79, 107, 112, 166, 103, 241, 24, 223, 239, 120, 198, 58, 60, 82, 128, 3, 184, 66, 143, 224, 145, 224, 81, 206, 163, 45, 63, 90, 168, 114, 59, 33, 159, 95, 28, 139, 123, 98, 125, 196, 15, 70, 194, 253, 54, 14, 109, 226, 71, 17, 161, 93, 186, 87, 244, 138, 20, 52, 123, 251, 26, 36, 17, 46, 52, 231, 232, 76, 31, 221, 84, 37, 216, 165, 212, 106, 197, 242, 98, 43, 39, 175, 254, 145, 190, 84, 118, 222, 187, 136, 120, 163, 236, 249]
NUM_WALLPAGES = 50          # pages 0..49 (tiles 1..25), then the 8 door pages
DOORWALL = 98
_cache = {}


def data():
    if 'd' not in _cache:
        d = wolfdata.Data(DATADIR, SRCDIR)
        pal = wl_view.build_view_palette(d)
        _cache['d'] = d
        _cache['pal'] = pal
        _cache['map'] = wl_view.map256(d, pal)
    return _cache['d'], _cache['pal'], _cache['map']


def tex_pages():
    return list(range(NUM_WALLPAGES)) + list(range(DOORWALL, DOORWALL + 8))


def page_w(p):
    if p < NUM_WALLPAGES:
        return p
    if DOORWALL <= p < DOORWALL + 8:
        return NUM_WALLPAGES + (p - DOORWALL)
    return 0xFF


def wall_textures():
    d, pal, m = data()
    out = []
    for p in tex_pages():
        cols = d.wall(p)
        buf = bytearray()
        for x in range(64):
            buf += bytes(texv(m[v]) for v in cols[x])
        out.append(bytes(buf))
    return out


def wall_tabs():
    """x side (vertical wall pages) and y side (horizontal) tables by tile"""
    xs, ys = bytearray([0xFF] * 64), bytearray([0xFF] * 64)
    for n in range(1, 64):
        hp = 2 * (n - 1)
        xs[n] = page_w(hp + 1)
        ys[n] = page_w(hp)
    return bytes(xs), bytes(ys)


WEAPON_SPRITES = range(416, 436)       # hardware sprites (wl_weapons.py)


def shapes():
    """list over sprite pages: None (empty page) or bytes:
    u8 width (64), u16 column offsets, segments (top, count, texels) .. 0xFF"""
    d, pal, m = data()
    out = []
    for s in range(d.num_sprites()):
        sp = d.sprite(s) if s not in WEAPON_SPRITES else None
        if sp is None:
            out.append(None)
            continue
        data_ = bytearray([64])
        data_ += bytes(128)
        for x in range(64):
            off = len(data_)
            data_[1 + 2 * x] = off & 0xFF
            data_[2 + 2 * x] = off >> 8
            col = sp[x]
            y = 0
            while y < 64:
                if col[y] is None:
                    y += 1
                    continue
                y0 = y
                while y < 64 and col[y] is not None:
                    y += 1
                data_ += bytes([y0, y - y0]) + bytes(texv(m[v]) for v in col[y0:y])
            data_.append(0xFF)
        out.append(bytes(data_))
    return out


def build_render(pk, H, C):
    """pack all renderer data; append header (H) and bank-0 table (C) lines"""
    walls = wall_textures()
    tbanks = pk.add_split('walltex', b''.join(walls), 0x4000)
    assert tbanks == list(range(tbanks[0], tbanks[0] + len(tbanks)))
    H.append('#define TEX_BANK0 %d' % tbanks[0])
    H.append('#define NUM_WALLTEX %d' % len(walls))
    H.append('#define DOORW_NORMAL %d' % (NUM_WALLPAGES + 0))
    H.append('#define DOORW_FRAME %d' % (NUM_WALLPAGES + 2))
    H.append('#define DOORW_ELEV %d' % (NUM_WALLPAGES + 4))
    H.append('#define DOORW_LOCK %d' % (NUM_WALLPAGES + 6))
    import math
    H.append('extern const u16 sin_q[91];')
    C.append('const u16 sin_q[91] = {%s};' % ','.join(str(min(65535, int(round(math.sin(math.radians(a)) * 65536)))) for a in range(91)))
    H.append('extern const u16 tan_q[46];')
    C.append('const u16 tan_q[46] = {%s};' % ','.join(str(min(65535, int(round(math.tan(math.radians(a)) * 65536)))) for a in range(46)))
    H.append('extern const u8 rndtable[256];')
    # only US_RndT (bank 1) reads it: a file of its own, linked into that bank (not 256 bytes of bank 0)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gen', 'rndtable.c'), 'w') as f:
        f.write('/* generated by tools/wl_assets.py */\n#include "assets.h"\nconst u8 rndtable[256] = {%s};\n' % ','.join(str(v) for v in RNDTABLE))
    d_, pal_, m_ = data()
    H.append('extern const u8 view_pal[16];')
    C.append('const u8 view_pal[16] = {%s};' % ','.join(str(b) for b in sms.pal_bytes(pal_)))
    H.append('#define CEIL_PAL_E1 %d' % sms.cram(sms.snap(d_.pal8[0xbf])))
    sbanks, stab = rg.build_scalers()
    sb = [pk.add_raw_bank('scalers%d' % i, b) for i, b in enumerate(sbanks)]
    ssbanks, sstab = rg.build_sprite_scalers()
    ssb = [pk.add_raw_bank('sprscalers%d' % i, b) for i, b in enumerate(ssbanks)]
    plb = pk.add_raw_bank('planes', rg.plane_bank())
    H.append('#define PLANE_BANK %d' % plb)
    fine_, colk_, colmin_ = rg.column_tables()
    divb = []
    for c in range(rg.NRAYS):
        k = colk_[c]
        assert (k >> 8) < 256
        divb.append(pk.add_raw_bank('divtab%d' % c,
                    bytes(min(rg.MAXS, k // dd) for dd in range(256, 16384))))
    assert divb == list(range(divb[0], divb[0] + rg.NRAYS))
    H.append('#define DIV_BANK0 %d' % divb[0])
    _, invq_ = rg.trig_quarter()
    sideb = []
    for a0 in range(0, 901, 32):
        buf = bytearray()
        for a in range(a0, min(a0 + 32, 901)):
            dl = invq_[a]
            for f in range(256):
                if dl == 0xFFFF:
                    v = 0xFFFF if f else 0
                else:
                    v = ((dl >> 8) * f + (((dl & 0xFF) * f) >> 8)) & 0xFFFF
                buf += bytes((v & 0xFF, v >> 8))
        sideb.append(pk.add_raw_bank('sidetab%d' % (a0 // 32), buf))
    assert sideb == list(range(sideb[0], sideb[0] + len(sideb)))
    H.append('#define SIDE_BANK0 %d' % sideb[0])
    shp = shapes()
    sh_bank, sh_addr = [], []
    for i, dd in enumerate(shp):
        if dd is None:
            sh_bank.append(0)
            sh_addr.append(0)
        else:
            b, a = pk.add('shape%d' % i, dd)
            sh_bank.append(b)
            sh_addr.append(a)
    C.append('const u8 shape_width[%d] = {%s};' % (len(shp), ','.join(str(dd[0] if dd else 0) for dd in shp)))
    H.append('extern const u8 shape_width[%d];   /* source columns, 0 = no shape */' % len(shp))
    rd = bytearray()
    syms = []

    def put(buf, name, data_, align=1):
        while len(buf) % align:
            buf.append(0)
        syms.append((name, 0x4000 + len(buf)))
        buf.extend(data_)
    sq = [i * i // 4 for i in range(512)]
    put(rd, 'RD_QSQ_LO', bytes(v & 0xFF for v in sq), 256)
    put(rd, 'RD_QSQ_HI', bytes(v >> 8 for v in sq), 256)
    cosq, invq = rg.trig_quarter()
    put(rd, 'RD_COSQ', u16le(cosq))
    put(rd, 'RD_INVQ', u16le(invq))
    fine, colk, colmin = rg.column_tables()
    put(rd, 'RD_COLFINE', u16le(fine))
    put(rd, 'RD_COLK', u16le(colk))
    put(rd, 'RD_COLMIN', u16le(colmin))
    put(rd, 'RD_SCALER_BANK', bytes(sb[b] for b, a in stab))
    put(rd, 'RD_SCALER_ADDR', u16le([a for b, a in stab]))
    put(rd, 'RD_SSCALER_BANK', bytes(ssb[b] for b, a in sstab))
    put(rd, 'RD_SSCALER_HI', bytes(a >> 8 for b, a in sstab))
    xs, ys = wall_tabs()
    put(rd, 'RD_WALLX', xs)
    put(rd, 'RD_WALLY', ys)
    rd2 = bytearray()
    import math
    atan = bytearray()
    for dy in range(-14, 15):                       # rows of 32 (index (dy + 14) * 32 + dx + 14: sprites_a.s)
        for dx in range(-14, 15):
            a = math.atan2(-dy, dx) / (2 * math.pi) * 256
            atan.append(int(round(a)) & 255)
        atan += bytes(3)
    put(rd2, 'RD2_ATAN', bytes(atan))
    put(rd2, 'RD2_STEPINV', u16le(rg.sprite_stepinv()))
    put(rd2, 'RD2_SHAPE_BANK', bytes(sh_bank))
    put(rd2, 'RD2_SHAPE_ADDR', u16le(sh_addr))
    rdb = pk.add_raw_bank('renderdata', rd)
    rdb2 = pk.add_raw_bank('renderdata2', rd2)
    H.append('#define RD_BANK %d' % rdb)
    H.append('#define RD2_BANK %d' % rdb2)
    for n, a in syms:
        H.append('#define %s 0x%04X' % (n, a))
    H.append('#define MAXS %d' % rg.MAXS)
    H.append('#define PROJ_KH %d' % round(rg.KH))
    H.append('#define PROJ_KV_HALF_256 %d' % round(rg.KV_HALF * 256))
    H.append('#define PROJ_FOV8 %d' % rg.HALF_FOV8)
    H.append('#define PROJ_HSCALE_NUM %d\n#define PROJ_HSCALE_DEN %d' % (5, 6))
    assert abs(rg.HSCALE - 5 / 6) < 1e-9
    H.append('#define NRAYS %d' % rg.NRAYS)
    H.append('#define VIEW_H %d' % rg.VIEW_H)
    H.append('#define VIEW_CY %d' % rg.CY)
    H.append('#define VIEW_TW %d' % (rg.VIEW_W // 8))
    H.append('#define VIEW_TH %d' % (rg.VIEW_H // 8))
    H.append('#define VIEW_TX0 %d' % ((32 - rg.VIEW_W // 8) // 2))     # screen tile column of the view (centred)
    H.append('#define VIEW_TY0 0')                                       # the status bar starts at row 12
    ncell = (rg.VIEW_W // 8) * (rg.VIEW_H // 8)
    H.append('#define TILE_SKY %d' % ncell)                             # flat tiles after the view's own tiles
    H.append('#define TILE_GROUND %d' % (ncell + 1))
    H.append('#define TILE_BLANK %d' % (ncell + 2))
    H.append('#define TILE_FREE %d' % (ncell + 3))
    # num = |q| * KH by shifts and adds, most significant bit first (sprites_a.s includes this)
    kh = int(round(rg.KH))
    assert abs(rg.KH - kh) < 1e-9
    H.append('#define KHMUL_KH %d' % kh)
    lines = [';; generated by tools/wl_assets.py: a:hl = |q| * %d (de = |q|), most significant bit first' % kh,
             '        ld      h,d', '        ld      l,e', '        xor     a,a']
    for bit in range(kh.bit_length() - 2, -1, -1):
        lines += ['        add     hl,hl', '        rla']
        if (kh >> bit) & 1:
            lines += ['        add     hl,de', '        adc     a,#0']
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gen', 'khmul.inc'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    H.append('#define NUMSCALEPICS %d' % len(shp))
    print('render data: RD %d RD2 %d bytes, tex banks %d..%d, shapes %d bytes' %
          (len(rd), len(rd2), tbanks[0], tbanks[-1], sum(len(x) for x in shp if x)))
