#!/usr/bin/env python3
"""The 3-D view has Wolfenstein 3D's own projection (72 degree field of view, a wall one tile away
and 1.2 tiles high is 158 px / distance): compare the wall extents of a ROM frame (the start of
E1M1, screenshot of smstest) with tools/refview.py, a float ray caster of the real textures.
usage: check_view.py <screenshot.ppm>   exit status 0 = the geometry matches"""
import sys, os
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import refview

# E1M1 start: (29.5, 57.5) looking east
ref = refview.render(29.5, 57.5, 0, W=192, H=96, KH=132.0, wall_k=158.4).astype(int)
rom = np.array(Image.open(sys.argv[1]).convert('RGB').crop((32, 0, 224, 96))).astype(int)
CEIL_ROM, FLOOR_ROM = (85, 85, 85), (170, 170, 170)        # palette entries 1 and 0
CEIL_REF, FLOOR_REF = (56, 56, 56), (113, 113, 113)


def extents(img, ceil, floor):
    def is_(px, c):
        return abs(px - np.array(c)).sum() < 12
    out = []
    for x in range(img.shape[1]):
        col = img[:, x]
        top = 0
        while top < len(col) and is_(col[top], ceil):
            top += 1
        bot = len(col)
        while bot > top and is_(col[bot - 1], floor):
            bot -= 1
        out.append((top, bot))
    return np.array(out)


er = extents(rom, CEIL_ROM, FLOOR_ROM)
ef = extents(ref, CEIL_REF, FLOOR_REF)
# the dead guard lying in the room hides the floor of a few columns of the ROM frame: compare the tops
tops = abs(er[:, 0] - ef[:, 0])
hr, hf = er[:, 1] - er[:, 0], ef[:, 1] - ef[:, 0]
print('wall top: mean |diff| %.2f px, max %d; wall height rom %.1f ref %.1f' % (tops.mean(), tops.max(), hr.mean(), hf.mean()))
ok = tops.mean() <= 3.0 and abs(hr.mean() - hf.mean()) <= 5.0
print('PASS' if ok else 'FAIL: the view geometry does not match the Wolfenstein 3D projection')
sys.exit(0 if ok else 1)
