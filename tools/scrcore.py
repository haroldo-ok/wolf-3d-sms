"""Full screen pictures for the SMS: scaling, palettes, tiles.

* scale_screen(): 320x200 -> 256x192 by dropping one column in five and one row in 25 where it costs the least
  (the SMS pixel is ~1.25x wider than a VGA one: this keeps the picture's proportions; it never blends colours, so
  text and pixel art stay crisp and every index of the result is an index of the original).
* quantize_photo(): two 16 colour palettes (every 8x8 cell picks one), exact SMS gamut, then tiles with flips;
  too many tiles -> the most similar ones are merged (perceptual error weighted by use).
* quantize_ui(): one 16 colour palette with fixed slots; palette 1 differs from palette 0 in the "ink" slots only,
  so a menu item is highlighted by switching the palette bit of its cells.
"""
import numpy as np
import sms


# ------------------------------------------------------------------ scaling
def _line_costs(p):
    """cost of removing each column of p (h x w): pixels that differ from the less different neighbour"""
    h, w = p.shape
    costs = np.zeros(w)
    for c in range(w):
        cs = []
        if c > 0:
            cs.append(np.count_nonzero(p[:, c] != p[:, c - 1]))
        if c < w - 1:
            cs.append(np.count_nonzero(p[:, c] != p[:, c + 1]))
        costs[c] = min(cs)
    return costs


def _choose(costs, k, gmin, gmax):
    """k indices to drop: consecutive gaps in [gmin, gmax], first < gmax, last >= n - gmax; minimal total cost"""
    n = len(costs)
    INF = 1e18
    dp = np.full((k, n), INF)
    back = np.zeros((k, n), np.int32)
    ideal = n / k
    for i in range(min(gmax, n)):
        dp[0][i] = costs[i] * 16 + abs(i - ideal / 2) * 0.01
    for j in range(1, k):
        prev = dp[j - 1]
        for i in range(n):
            lo, hi = i - gmax, i - gmin
            if hi < 0:
                continue
            lo = max(lo, 0)
            seg = prev[lo:hi + 1]
            a = int(np.argmin(seg))
            if seg[a] >= INF:
                continue
            dp[j][i] = seg[a] + costs[i] * 16 + abs(i - (j + 0.5) * ideal) * 0.01
            back[j][i] = lo + a
    last = dp[k - 1][max(0, n - gmax):]
    i = max(0, n - gmax) + int(np.argmin(last))
    out = [i]
    for j in range(k - 1, 0, -1):
        i = back[j][i]
        out.append(i)
    return sorted(out)


def _bands(p):
    """row bands separated by rows of one colour"""
    h = p.shape[0]
    uni = [len(np.unique(p[r])) == 1 for r in range(h)]
    bands, s = [], None
    for r in range(h + 1):
        if r < h and not uni[r]:
            if s is None:
                s = r
        elif s is not None:
            bands.append((s, r))
            s = None
    return bands, uni


def scale_screen(pix, out_w=256, out_h=192):
    """index image (h x w) -> out_h x out_w; also returns the kept source rows and, per output row, the kept columns"""
    p = np.array(pix, dtype=np.uint8)
    h, w = p.shape
    k = h - out_h
    keep_rows = list(range(h))
    if k:
        drop = set(_choose(_line_costs(p.T), k, h // k - 8, h // k + 8))
        keep_rows = [r for r in range(h) if r not in drop]
        p = p[keep_rows, :]
    h = p.shape[0]
    k = w - out_w
    out = np.zeros((h, out_w), np.uint8)
    bands, uni = _bands(p)
    for r in range(h):
        if uni[r]:
            out[r, :] = p[r, 0]
    colmap = np.zeros((h, out_w), np.int32)
    for r in range(h):
        colmap[r] = np.arange(out_w) * w // out_w
    for (a, b) in bands:
        seg = p[a:b]
        drop = set(_choose(_line_costs(seg), k, 3, 7))
        keep = [c for c in range(w) if c not in drop]
        out[a:b] = seg[:, keep]
        colmap[a:b] = keep
    return out, keep_rows, colmap


# ------------------------------------------------------------- tiles
def planar(t):
    return sms.tile_to_planar(t)


def _variants(t):
    yield t.tobytes(), 0
    yield t[:, ::-1].tobytes(), 0x200
    yield t[::-1, :].tobytes(), 0x400
    yield t[::-1, ::-1].tobytes(), 0x600


def _flip(t, fl):
    if fl & 0x200:
        t = t[:, ::-1]
    if fl & 0x400:
        t = t[::-1, :]
    return t


def pack_tiles(cidx, cpal, pal_rgb, cell_rgb, limit, allow_flip=True, protect=None):
    """cidx (th, tw, 8, 8) colour indices, cpal (th, tw) palette of each cell, pal_rgb (2, 16, 3),
    cell_rgb (th, tw, 8, 8, 3) the wanted picture.  -> tiles (list of 8x8), nt (th x tw of (tile, flags with the palette bit))
    protect: bool (th, tw) cells whose tiles are never merged away (text)"""
    th, tw = cidx.shape[:2]
    uniq, counts, key2 = [], [], {}
    cmap = [[None] * tw for _ in range(th)]
    upal = []
    prot = {}
    for y in range(th):
        for x in range(tw):
            t = cidx[y, x]
            hit = None
            if allow_flip:
                for k, fl in _variants(t):
                    if k in key2:
                        hit = (key2[k], fl)
                        break
            else:
                k = t.tobytes()
                if k in key2:
                    hit = (key2[k], 0)
            if hit is None:
                key2[t.tobytes()] = len(uniq)
                uniq.append(t.copy())
                counts.append(0)
                upal.append(int(cpal[y, x]))
                hit = (len(uniq) - 1, 0)
            counts[hit[0]] += 1
            cmap[y][x] = hit
            if protect is not None and protect[y, x]:
                prot[hit[0]] = max(prot.get(hit[0], 0), float(protect[y, x]) if protect[y, x] is not True else 1000.0)
    n = len(uniq)
    tiles = uniq
    if n > limit:
        rgbn = np.asarray(pal_rgb, dtype=np.int64)
        feat = np.stack([rgbn[upal[k]][uniq[k]].reshape(-1) for k in range(n)]).astype(np.int64)
        sq = (feat * feat).sum(1)
        dist = (sq[:, None] + sq[None, :] - 2 * feat @ feat.T).astype(np.float64)
        lum = (feat.reshape(n, 64, 3) * np.array([3, 6, 1])).sum(2) / 10.0
        sd = lum.std(1)
        edge = (np.abs(np.diff(lum.reshape(n, 8, 8), axis=1)).sum((1, 2)) +
                np.abs(np.diff(lum.reshape(n, 8, 8), axis=2)).sum((1, 2)))
        dist = dist + 64 * (sd[:, None] - sd[None, :]) ** 2 + (edge[:, None] - edge[None, :]) ** 2 / 64.0
        up = np.array(upal)
        dist[up[:, None] != up[None, :]] = 1e30        # only tiles drawn with the same palette are merged
        alive = np.ones(n, bool)
        imp = np.array([(20.0 if len(np.unique(u)) <= 2 else 1.0) * (1.0 + prot.get(k, 0.0))
                        for k, u in enumerate(uniq)])
        cnt = np.array(counts, np.float64) * imp
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
                if tiles[c] is uniq[k] or np.array_equal(tiles[c], uniq[k]):
                    cmap[y][x] = (c, fl)
                    continue
                best = None
                pb = int(cpal[y, x])
                for kk, f in _variants(tiles[c]):
                    v = np.frombuffer(kk, np.uint8).reshape(8, 8)
                    e = int(((rgbn[pb][v] - cell_rgb[y, x]) ** 2).sum())
                    if best is None or e < best[0]:
                        best = (e, f)
                    if not allow_flip:
                        break
                cmap[y][x] = (int(c), best[1])
        # palette bits of the unmerged cells stay as chosen
    nt = [[(cmap[y][x][0], cmap[y][x][1] | (0x800 if cpal[y, x] else 0)) for x in range(tw)] for y in range(th)]
    return tiles, nt


# --------------------------------------------------------- photo palettes
SAL_POW = 0.6
SAL_CHROMA = 30.0

def quantize_photo(idx, pal8, limit, iters=8, fixed=(), protect_cells=()):
    """idx: 192 x 256 VGA indices -> dict(tiles, nt, pal [2][16 cram bytes], preview rgb)"""
    th, tw = idx.shape[0] // 8, idx.shape[1] // 8
    rgb = np.asarray(pal8, np.uint8)[idx]
    flat = rgb.reshape(-1, 3)
    uniq, inv, cnt = np.unique(flat, axis=0, return_inverse=True, return_counts=True)
    inv = inv.reshape(-1)
    lab = sms.rgb_to_lab(uniq)
    cell_ids = inv.reshape(th, 8, tw, 8).transpose(0, 2, 1, 3).reshape(th, tw, 64)
    cell_rgb = rgb.reshape(th, 8, tw, 8, 3).transpose(0, 2, 1, 3, 4)
    # start: split the cells by luminance
    cell_l = lab[cell_ids][..., 0].mean(-1)
    assign = (cell_l > np.median(cell_l)).astype(int)
    pal = [None, None]
    for it in range(iters):
        for g in (0, 1):
            m = assign == g
            if not m.any():
                m = np.ones_like(m)
            ids = cell_ids[m].reshape(-1)
            w = np.bincount(ids, minlength=len(uniq)).astype(np.float64)
            sel = np.nonzero(w)[0]
            # rare saturated colours (skin) matter more to the eye than their pixel count says
            chroma = np.hypot(lab[sel, 1], lab[sel, 2])
            pal[g] = sms.choose_palette(uniq[sel], w[sel] ** SAL_POW * (1 + chroma / SAL_CHROMA), 16, fixed=fixed)[0]
        best = []
        arg = []
        for g in (0, 1):
            pl = sms.palette_lab(pal[g])
            d = ((lab[:, None, :] - pl[None, :, :]) ** 2).sum(-1)
            best.append(d.min(1))
            arg.append(d.argmin(1))
        err = np.stack([best[g][cell_ids].sum(-1) for g in (0, 1)])
        new = err.argmin(0)
        if (new == assign).all() and it > 0:
            break
        assign = new
    cidx = np.zeros((th, tw, 8, 8), np.uint8)
    for g in (0, 1):
        m = assign == g
        cidx[m] = arg[g][cell_ids[m]].reshape(-1, 8, 8)
    pal_rgb = np.stack([sms.pal_rgb(pal[0]), sms.pal_rgb(pal[1])]).astype(np.int64)
    protect = np.zeros((th, tw))
    for cell in protect_cells:
        c0, r0, c1, r1 = cell[:4]
        protect[r0:r1 + 1, c0:c1 + 1] = np.maximum(protect[r0:r1 + 1, c0:c1 + 1], cell[4] if len(cell) > 4 else 1000.0)
    tiles, nt = pack_tiles(cidx, assign.copy(), pal_rgb, cell_rgb.astype(np.int64), limit, protect=protect)
    return dict(tiles=tiles, nt=nt, pal=[sms.pal_bytes(pal[0]), sms.pal_bytes(pal[1])], pal_rgb=pal_rgb)


# ------------------------------------------------------------- ui palette
def quantize_ui(idx, pal8, fixed, slot_of, extra_rgb=(), extra_w=(), protect_idx=(), limit=432, hi=None, protect_cells=()):
    """idx: 192 x 256 VGA indices.  fixed: list of SMS triples for slots 0.. (palette 0).  slot_of: {vga index: slot}
    for the indices that must keep a slot of their own.  hi: {slot: triple} the colours of palette 1 (the ink slots).
    extra_rgb/extra_w: more picture colours (a sprite) that the palette must serve."""
    pal8 = np.asarray(pal8, np.uint8)
    used, counts = np.unique(idx, return_counts=True)
    cols, ws = [], []
    for u, c in zip(used, counts):
        if int(u) in slot_of:
            continue
        cols.append(pal8[u])
        ws.append(float(c))
    for c, w in zip(extra_rgb, extra_w):
        cols.append(np.asarray(c, np.uint8))
        ws.append(float(w))
    cols = np.array(cols, np.uint8).reshape(-1, 3)
    ws = np.array(ws)
    # merge equal colours
    if len(cols):
        u, inv = np.unique(cols, axis=0, return_inverse=True)
        w2 = np.zeros(len(u))
        np.add.at(w2, inv.reshape(-1), ws)
        cols, ws = u, w2
    pal0 = sms.choose_palette(cols, ws, 16, fixed=fixed)[0] if len(cols) else list(fixed) + [(0, 0, 0)] * (16 - len(fixed))
    pal1 = list(pal0)
    for s, c in (hi or {}).items():
        pal1[s] = tuple(c)
    pl = sms.palette_lab(pal0)
    # every VGA index -> slot
    lut = np.zeros(256, np.uint8)
    for u in range(256):
        if u in slot_of:
            lut[u] = slot_of[u]
        else:
            d = ((sms.rgb_to_lab(pal8[u])[None, :] - pl) ** 2).sum(1)
            lut[u] = int(np.argmin(d))
    th, tw = idx.shape[0] // 8, idx.shape[1] // 8
    ci = lut[idx]
    cidx = ci.reshape(th, 8, tw, 8).transpose(0, 2, 1, 3).copy()
    pal_rgb = np.stack([sms.pal_rgb(pal0), sms.pal_rgb(pal1)]).astype(np.int64)
    cell_rgb = pal_rgb[0][cidx]
    cpal = np.zeros((th, tw), int)
    protect = np.zeros((th, tw), bool)
    for pi in protect_idx:
        m = (idx == pi).reshape(th, 8, tw, 8).any(axis=(1, 3))
        protect |= m
    protect = protect.astype(float) * 1000.0
    for cell in protect_cells:
        c0, r0, c1, r1 = cell[:4]
        protect[r0:r1 + 1, c0:c1 + 1] = np.maximum(protect[r0:r1 + 1, c0:c1 + 1], cell[4] if len(cell) > 4 else 1000.0)
    tiles, nt = pack_tiles(cidx, cpal, pal_rgb, cell_rgb, limit, protect=protect)
    return dict(tiles=tiles, nt=nt, pal=[sms.pal_bytes(pal0), sms.pal_bytes(pal1)], pal_rgb=pal_rgb, lut=lut,
                pal0=pal0, pal1=pal1)


def render_screen(tiles, nt, pal_rgb, nt_override=None):
    """preview image (RGB) of a tile map"""
    th, tw = len(nt), len(nt[0])
    out = np.zeros((th * 8, tw * 8, 3), np.uint8)
    for y in range(th):
        for x in range(tw):
            i, fl = nt[y][x]
            t = _flip(tiles[i], fl & 0x600)
            out[y * 8:y * 8 + 8, x * 8:x * 8 + 8] = np.asarray(pal_rgb[1 if fl & 0x800 else 0], np.uint8)[t]
    return out
