#!/usr/bin/env python3
"""High scores live in cartridge RAM: a name is entered (debug hook), the game is quit from its menu (a soft reset:
everything in system RAM is gone), and the table shown afterwards still has the name.
Checks that the reset really happened (game_frames went back down) and that the table rows are the same pictures.
exit status 0 = passed"""
import os, re, subprocess, sys
import numpy as np
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TMP = '/tmp/s'
os.makedirs(TMP, exist_ok=True)
bcall = re.search(r'^\s*([0-9A-Fa-f]+)\s+___sdcc_bcall_ehl', open(os.path.join(ROOT, 'build_dbg/game.map')).read(), re.M).group(1).lstrip('0')
script = '''symbols build_dbg/game.noi
run 200
waituntil game_frames >= 3 20000 w2
poke dbg_uireq 3 w1
run 150
tap up
run 10
tap right
run 10
tap up
run 10
tap 1
run 150
screenshot %(t)s/ts1.ppm
tap 1
run 200
poke dbg_wx 33 w1
poke dbg_wy 21 w1
poke dbg_wdir 0 w1
poke dbg_warp 1 w1
run 80
print dbg_tx w1
pause
run 150
''' + 'tap down\nrun 12\n' * 7 + '''tap 1
run 100
tap up
run 12
tap 1
run 700
print dbg_tx w1
poke dbg_uireq 7 w1
run 200
screenshot %(t)s/ts2.ppm
'''
script = script.replace('%(t)s', TMP)
open(TMP + '/t_scores.txt', 'w').write(script)
out = subprocess.run([os.path.join(ROOT, 'tools/smstest/smstest'), os.path.join(ROOT, 'wolf3d_dbg.sms'), TMP + '/t_scores.txt'], cwd=ROOT,
                     capture_output=True, text=True, env=dict(os.environ, SMSTEST_BCALL='0x' + bcall)).stdout
tx = [int(x) for x in re.findall(r'print: dbg_tx = (\d+)', out)]
fails = []
if 'RESULT: ALL PASSED' not in out or len(tx) != 2:
    print('FAIL the script did not run clean:\n' + out[-600:])
    fails.append('the script did not run clean')
else:
    # the player was warped to x = 33; after the quit the program starts again: a new game, the player on the start tile (29)
    ok = tx[0] == 33 and tx[1] == 29
    print(('PASS ' if ok else 'FAIL ') + 'reset: the player was at x = %d before the quit, at x = %d after it (the start tile is 29)' % tuple(tx))
    if not ok:
        fails.append('no reset')
    a = np.array(Image.open(TMP + '/ts1.ppm').convert('RGB')).astype(int)
    b = np.array(Image.open(TMP + '/ts2.ppm').convert('RGB')).astype(int)
    rows = (slice(56, 180), slice(0, 256))
    diff = int((np.abs(a[rows] - b[rows]).sum(2) > 30).sum())
    print(('PASS ' if diff == 0 else 'FAIL ') + 'the new entry and all the rows below it are the same table after the reset (%d differing pixels)' % diff)
    if diff:
        fails.append('table differs')
sys.exit(1 if fails else 0)
