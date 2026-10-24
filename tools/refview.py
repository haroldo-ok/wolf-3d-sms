"""Reference (float) Wolf3D ray caster used to check the SMS renderer's projection.

  python3 refview.py out.png px py angle_deg [KH] [wall_k]

KH: horizontal focal length in view pixels; wall_k: wall height in pixels
at distance 1 tile (full height).  The original Wolf3D (view width W): KH =
W/2 * 1.375 and wall_k = 1.2 * KH on square pixels.
"""
import sys, math
import numpy as np
from PIL import Image
import wl_assets


def render(px, py, ang_deg, W=192, H=96, KH=132.0, wall_k=158.4, map_index=0, back=0.34375):
    d, pal, m = wl_assets.data()
    pl0 = d.maps[map_index]['planes'][0]
    walls = {}

    def tex(page):
        if page not in walls:
            walls[page] = np.array(d.wall(page), dtype=np.uint8)      # [x][y]
        return walls[page]
    pal8 = np.array(d.pal8, dtype=np.uint8)
    a0 = math.radians(ang_deg)
    # viewer sits `back` tiles behind the player
    vx = px - math.cos(a0) * back
    vy = py + math.sin(a0) * back
    img = np.zeros((H, W, 3), dtype=np.uint8)
    img[:H // 2, :] = (56, 56, 56)
    img[H // 2:, :] = (113, 113, 113)
    cy = H / 2
    for sx in range(W):
        off = math.atan((sx + 0.5 - W / 2) / KH)
        a = a0 - off
        dx, dy = math.cos(a), -math.sin(a)
        mx, my = int(vx), int(vy)
        ddx = abs(1 / dx) if dx else 1e30
        ddy = abs(1 / dy) if dy else 1e30
        if dx < 0: stepx, sdx = -1, (vx - mx) * ddx
        else: stepx, sdx = 1, (mx + 1 - vx) * ddx
        if dy < 0: stepy, sdy = -1, (vy - my) * ddy
        else: stepy, sdy = 1, (my + 1 - vy) * ddy
        side = 0
        for _ in range(80):
            if sdx < sdy:
                sdx += ddx; mx += stepx; side = 0
            else:
                sdy += ddy; my += stepy; side = 1
            if not (0 <= mx < 64 and 0 <= my < 64):
                break
            v = pl0[my * 64 + mx]
            if v == 0 or v >= 64 and not (90 <= v <= 101):
                if not (1 <= v <= 63) and not (90 <= v <= 101):
                    continue
            if 1 <= v <= 63:
                dist = (sdx - ddx) if side == 0 else (sdy - ddy)
                if side == 0:
                    hy = vy + dist * dy
                    u = hy - math.floor(hy)
                    page = 2 * (v - 1) + 1
                    if stepx < 0: u = 1 - u
                else:
                    hx = vx + dist * dx
                    u = hx - math.floor(hx)
                    page = 2 * (v - 1)
                    if stepy > 0: u = 1 - u
            elif 90 <= v <= 101:
                # thin door in the middle of the tile (closed)
                vertical = (v & 1) == 0
                if vertical:
                    if side != 0: continue
                    dist = (sdx - ddx) + 0.5 * ddx
                    hy = vy + dist * dy
                    if math.floor(hy) != my: continue
                    u = hy - math.floor(hy)
                    page = 98 if v < 92 else (104 if v < 100 else 102)
                else:
                    if side != 1: continue
                    dist = (sdy - ddy) + 0.5 * ddy
                    hx = vx + dist * dx
                    if math.floor(hx) != mx: continue
                    u = hx - math.floor(hx)
                    page = 99 if v < 92 else (105 if v < 100 else 103)
            else:
                continue
            perp = dist * math.cos(off)
            if perp < 0.05: perp = 0.05
            h = wall_k / perp
            top = cy - h / 2
            t = tex(page)
            col = t[min(63, int(u * 64))]
            ys = np.arange(H)
            r = ((ys + 0.5 - top) * 64 / h).astype(int)
            ok = (r >= 0) & (r < 64)
            img[ok, sx] = pal8[col[r[ok]]]
            break
    return img


if __name__ == '__main__':
    out = sys.argv[1]
    px, py, ang = float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
    KH = float(sys.argv[5]) if len(sys.argv) > 5 else 132.0
    wk = float(sys.argv[6]) if len(sys.argv) > 6 else 1.2 * KH
    im = render(px, py, ang, KH=KH, wall_k=wk)
    Image.fromarray(im).resize((192 * 3, 96 * 3), Image.NEAREST).save(out)
