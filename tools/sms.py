"""Colour and tile helpers for the Sega Master System (2 bits per channel).

* SMS colours: 64 (r,g,b in 0..3), CRAM byte = r | g << 2 | b << 4
* Lab k-means palettes snapped to that gamut
* 8x8 planar tile packing, tile de-duplication with a tile budget (used for
  every full screen picture), simple 2-D helpers
"""
import numpy as np

LEVELS = np.array([0, 85, 170, 255], dtype=np.float64)


def rgb_to_lab(rgb):
    rgb = np.asarray(rgb, dtype=np.float64) / 255.0
    a = np.where(rgb > 0.04045, ((rgb + 0.055) / 1.055) ** 2.4, rgb / 12.92)
    m = np.array([[0.4124564, 0.3575761, 0.1804375],
                  [0.2126729, 0.7151522, 0.0721750],
                  [0.0193339, 0.1191920, 0.9503041]])
    xyz = a @ m.T
    xyz /= np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16.0 / 116.0)
    L = 116 * f[..., 1] - 16
    A = 500 * (f[..., 0] - f[..., 1])
    B = 200 * (f[..., 1] - f[..., 2])
    return np.stack([L, A, B], axis=-1)


# all 64 SMS colours
SMS = np.array([(r, g, b) for b in range(4) for g in range(4) for r in range(4)])
SMS_RGB = LEVELS[SMS]
SMS_LAB = rgb_to_lab(SMS_RGB)


def cram(c3):
    r, g, b = [int(v) for v in c3]
    return r | (g << 2) | (b << 4)


def cram_to_rgb(v):
    return (int(LEVELS[v & 3]), int(LEVELS[(v >> 2) & 3]), int(LEVELS[(v >> 4) & 3]))


def snap(rgb):
    """nearest SMS colour (2-bit triple) for an 8 bit rgb triple"""
    lab = rgb_to_lab(np.asarray(rgb, dtype=np.float64))
    i = np.argmin(((SMS_LAB - lab) ** 2).sum(axis=1))
    return tuple(int(v) for v in SMS[i])


def kmeans_palette(colors, weights, k, fixed=(), iters=40, seed=1):
    """colors: (n,3) 8-bit rgb of distinct colours, weights (n,)
    fixed: list of 2-bit triples that must be present (they stay first).
    returns list of k 2-bit triples."""
    rng = np.random.RandomState(seed)
    lab = rgb_to_lab(colors)
    w = np.asarray(weights, dtype=np.float64)
    fixedlab = [rgb_to_lab(LEVELS[np.array(f)]) for f in fixed]
    nf = len(fixed)
    nfree = k - nf
    cent = []
    idx = rng.choice(len(lab), p=w / w.sum())
    cent.append(lab[idx])
    allc = list(fixedlab) + cent
    for _ in range(nfree - 1):
        d = np.min(((lab[:, None, :] - np.array(allc)[None, :, :]) ** 2).sum(axis=2), axis=1)
        p = d * w
        if p.sum() <= 0:
            p = w
        idx = rng.choice(len(lab), p=p / p.sum())
        cent.append(lab[idx])
        allc = list(fixedlab) + cent
    cent = np.array(cent)
    for _ in range(iters):
        allc = np.array(list(fixedlab) + list(cent))
        d = ((lab[:, None, :] - allc[None, :, :]) ** 2).sum(axis=2)
        a = np.argmin(d, axis=1)
        newc = cent.copy()
        for j in range(nfree):
            m = a == nf + j
            if m.any():
                newc[j] = (lab[m] * w[m, None]).sum(axis=0) / w[m].sum()
        sn = ((newc[:, None, :] - SMS_LAB[None, :, :]) ** 2).sum(axis=2)
        newc = SMS_LAB[np.argmin(sn, axis=1)]
        if np.allclose(newc, cent):
            break
        cent = newc
    out = [tuple(f) for f in fixed]
    sn = ((cent[:, None, :] - SMS_LAB[None, :, :]) ** 2).sum(axis=2)
    for i in np.argmin(sn, axis=1):
        out.append(tuple(int(v) for v in SMS[i]))
    return out


def palette_lab(pal):
    return rgb_to_lab(LEVELS[np.array(pal)])


def nearest_index(lab_colors, pal):
    pl = palette_lab(pal)
    d = ((lab_colors[:, None, :] - pl[None, :, :]) ** 2).sum(axis=2)
    return np.argmin(d, axis=1), d.min(axis=1)


def best_kmeans(colors, weights, k, fixed=(), tries=6):
    best = None
    lab = rgb_to_lab(colors)
    w = np.asarray(weights, dtype=np.float64)
    for s in range(tries):
        p = kmeans_palette(colors, w, k, fixed, seed=s + 1)
        _, err = nearest_index(lab, p)
        e = float((err * w).sum() / w.sum())
        if best is None or e < best[0]:
            best = (e, p)
    return best[1], best[0]


def pal_bytes(pal):
    return bytes(cram(c) for c in pal)


def pal_rgb(pal):
    return np.array([cram_to_rgb(cram(c)) for c in pal], dtype=np.uint8)


# ------------------------------------------------------------------ tiles
def tile_to_planar(t):
    """8x8 array of colour indices (0..15) -> 32 bytes (4 bit planes per row)"""
    out = bytearray()
    for row in t:
        for b in range(4):
            v = 0
            for x in range(8):
                if int(row[x]) >> b & 1:
                    v |= 0x80 >> x
            out.append(v)
    return bytes(out)


def image_to_tiles_planar(img):
    """H x W index image (multiples of 8) -> bytes, tiles in row-major order"""
    H, W = img.shape
    out = bytearray()
    for ty in range(H // 8):
        for tx in range(W // 8):
            out += tile_to_planar(img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8])
    return bytes(out)


def tile_variants(t):
    yield t.tobytes(), 0
    yield t[:, ::-1].tobytes(), 0x200      # H flip
    yield t[::-1, :].tobytes(), 0x400      # V flip
    yield t[::-1, ::-1].tobytes(), 0x600


def quantize_tiles(p, limit, rgb, allow_flip=True, protect=20.0):
    """p: H x W colour index image (multiples of 8); rgb: 16 x 3 palette used
    for the perceptual error when tiles have to be merged.  Returns (list of
    8x8 index arrays, map of rows of (tile index, flip flags))."""
    H, W = p.shape
    th, tw = H // 8, W // 8
    rgbn = np.asarray(rgb, dtype=np.int32)
    cells = [[p[y * 8:y * 8 + 8, x * 8:x * 8 + 8].copy() for x in range(tw)] for y in range(th)]
    uniq, counts, key2 = [], [], {}
    cmap = [[None] * tw for _ in range(th)]
    for y in range(th):
        for x in range(tw):
            t = cells[y][x]
            hit = None
            if allow_flip:
                for k, fl in tile_variants(t):
                    if k in key2:
                        hit = (key2[k], fl)
                        break
            else:
                k = t.tobytes()
                if k in key2:
                    hit = (key2[k], 0)
            if hit is None:
                key2[t.tobytes()] = len(uniq)
                uniq.append(t)
                counts.append(0)
                hit = (len(uniq) - 1, 0)
            counts[hit[0]] += 1
            cmap[y][x] = hit
    n = len(uniq)
    if n <= limit:
        return uniq, cmap
    # lossy stage: merge the most similar tiles (use count weighted)
    feat = np.stack([rgbn[u].reshape(-1) for u in uniq]).astype(np.int64)      # n x 192
    sq = (feat * feat).sum(1)
    dist = (sq[:, None] + sq[None, :] - 2 * feat @ feat.T).astype(np.float64)
    lum = (feat.reshape(n, 64, 3) * np.array([3, 6, 1])).sum(2) / 10.0
    sd = lum.std(1)
    edge = (np.abs(np.diff(lum.reshape(n, 8, 8), axis=1)).sum((1, 2)) +
            np.abs(np.diff(lum.reshape(n, 8, 8), axis=2)).sum((1, 2)))
    dist = dist + 64 * (sd[:, None] - sd[None, :]) ** 2 + (edge[:, None] - edge[None, :]) ** 2 / 64.0
    alive = np.ones(n, bool)
    cnt = np.array(counts, np.float64)
    imp = np.array([(protect if len(np.unique(u)) <= 2 else 1.0) for u in uniq])
    cnt = cnt * imp
    target = list(range(n))
    BIG = 1e30
    np.fill_diagonal(dist, BIG)
    while alive.sum() > limit:
        w = np.minimum(cnt[:, None], cnt[None, :])
        cost = dist * w
        cost[~alive, :] = BIG
        cost[:, ~alive] = BIG
        i, j = np.unravel_index(np.argmin(cost), cost.shape)
        if cnt[i] < cnt[j]:
            i, j = j, i
        alive[j] = False
        cnt[i] += cnt[j]
        target[j] = i
        dist[j, :] = BIG
        dist[:, j] = BIG

    def resolve(k):
        while target[k] != k:
            k = target[k]
        return k
    keep = [k for k in range(n) if alive[k]]
    cid = {k: i for i, k in enumerate(keep)}
    tiles = [uniq[k].copy() for k in keep]
    for y in range(th):
        for x in range(tw):
            k, fl = cmap[y][x]
            c = cid[resolve(k)]
            best = None
            for kk, f in tile_variants(tiles[c]):
                v = np.frombuffer(kk, np.uint8).reshape(8, 8)
                e = int(((rgbn[v] - rgbn[cells[y][x]]) ** 2).sum())
                if best is None or e < best[0]:
                    best = (e, f)
                if not allow_flip:
                    break
            cmap[y][x] = (int(c), best[1])
    return tiles, cmap


def render_tiles(tiles, cmap):
    th = len(cmap)
    tw = len(cmap[0])
    out = np.zeros((th * 8, tw * 8), np.uint8)
    for y in range(th):
        for x in range(tw):
            i, fl = cmap[y][x]
            t = tiles[i]
            if fl & 0x200:
                t = t[:, ::-1]
            if fl & 0x400:
                t = t[::-1, :]
            out[y * 8:y * 8 + 8, x * 8:x * 8 + 8] = t
    return out


def save_png(idx, rgb, path, scale=2):
    from PIL import Image
    a = np.asarray(idx, dtype=np.uint8)
    pal = np.asarray(rgb, dtype=np.uint8)
    im = Image.fromarray(pal[a], 'RGB')
    if scale != 1:
        im = im.resize((im.width * scale, im.height * scale), Image.NEAREST)
    im.save(path)


def choose_palette(colors, weights, k, fixed=()):
    """exact-gamut palette: choose k of the 64 SMS colours minimising the
    weighted Lab error (greedy start, then swap search).  `fixed` colours
    (2-bit triples) always stay in the palette (first entries)."""
    lab = rgb_to_lab(colors)
    w = np.asarray(weights, dtype=np.float64)
    D = ((lab[:, None, :] - SMS_LAB[None, :, :]) ** 2).sum(axis=2)       # n x 64
    fidx = [int(np.nonzero((SMS == np.array(f)).all(axis=1))[0][0]) for f in fixed]
    sel = list(fidx)

    def cost(s):
        return float((D[:, s].min(axis=1) * w).sum())
    while len(sel) < k:
        best = None
        for c in range(64):
            if c in sel:
                continue
            e = cost(sel + [c])
            if best is None or e < best[0]:
                best = (e, c)
        sel.append(best[1])
    cur = cost(sel)
    improved = True
    while improved:
        improved = False
        for pos in range(len(fidx), k):
            for c in range(64):
                if c in sel:
                    continue
                t = list(sel)
                t[pos] = c
                e = cost(t)
                if e < cur - 1e-9:
                    cur, sel, improved = e, t, True
    return [tuple(int(v) for v in SMS[i]) for i in sel], cur / w.sum()
