#!/usr/bin/env python3
"""Regenerates docs/*.png (the README screenshots) from the debug ROM with smstest.

  python3 tools/docshots.py       (needs wolf3d_dbg.sms: make DEBUG=1)
"""
import os, subprocess
from PIL import Image
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
TMP = '/tmp/s'
os.makedirs(TMP, exist_ok=True)


def run(name, lines):
    p = os.path.join(TMP, 'docs_%s.txt' % name)
    open(p, 'w').write('symbols build_dbg/game.noi\nrun 300\n' + '\n'.join(lines) + '\n')
    out = subprocess.run(['bash', 'tools/st.sh', 'wolf3d_dbg.sms', p], cwd=ROOT, capture_output=True, text=True).stdout
    assert 'RESULT: ALL PASSED' in out, out[-500:]


def png(src, dst, scale=3):
    im = Image.open(os.path.join(TMP, src)).convert('RGB')
    im.resize((im.width * scale, im.height * scale), Image.NEAREST).save(os.path.join(ROOT, 'docs', dst))


run('start', ['screenshot %s/docs_start.ppm' % TMP])
png('docs_start.ppm', 'start.png')

# a guard comes at the player (no god mode: he shoots: the face, the health)
run('fight', ['poke dbg_wx 33 w1', 'poke dbg_wy 13 w1', 'poke dbg_wdir 0 w1', 'poke dbg_warp 1 w1', 'run 150',
              'screenshot %s/docs_fight.ppm' % TMP])
png('docs_fight.ppm', 'fight.png')

# the secret room of E1M1: clips and the machine gun picked up (the weapon in the status bar)
run('pickup', ['poke dbg_wx 28 w1', 'poke dbg_wy 22 w1', 'poke dbg_wdir 2 w1', 'poke dbg_warp 1 w1', 'run 30',
               'hold up', 'run 200', 'hold none', 'run 60', 'screenshot %s/docs_pickup.ppm' % TMP])
png('docs_pickup.ppm', 'pickup.png')

# the first view of all ten levels
lines = []
for n in range(1, 11):
    lines += ['poke dbg_map %d w1' % n, 'run 260', 'screenshot %s/docs_lv%02d.ppm' % (TMP, n)]
run('levels', lines)
W, H = 256, 192
sheet = Image.new('RGB', (W * 5, H * 2))
for n in range(1, 11):
    im = Image.open(os.path.join(TMP, 'docs_lv%02d.ppm' % n)).convert('RGB')
    sheet.paste(im, (((n - 1) % 5) * W, ((n - 1) // 5) * H))
sheet.resize((sheet.width * 2 // 2 * 1, sheet.height), Image.NEAREST).save(os.path.join(ROOT, 'docs', 'levels.png'))

# ------------------------------------------------------------- the front end
def run_rom(rom, noi, lines):
    p = os.path.join(TMP, 'docs_ui.txt')
    open(p, 'w').write('symbols %s\n' % noi + '\n'.join(lines) + '\n')
    out = subprocess.run(['bash', 'tools/st.sh', rom, p], cwd=ROOT, capture_output=True, text=True).stdout
    assert 'RESULT: ALL PASSED' in out, out[-500:]


shots = []


def shot(name):
    shots.append(name)
    return 'screenshot %s/ui_%s.ppm' % (TMP, name)


# the release ROM: from power on to the new game
run_rom('wolf3d.sms', 'build/game.noi', [
    'run 110', shot('pg13'), 'run 300', shot('title'), 'run 540', shot('credits'), 'tap 1', 'run 100', shot('menu'),
    'tap down', 'run 12', 'tap 1', 'run 100', shot('sound'), 'tap 2', 'run 100', 'tap up', 'run 12', 'tap 1', 'run 100',
    shot('difficulty'), 'tap 1', 'run 80', shot('psyched')])


# the debug ROM: the in-game menu, then one fresh run for each screen that follows a level
def hooked(n, extra, name, wait=500):
    run_rom('wolf3d_dbg.sms', 'build_dbg/game.noi', ['run 200', 'waituntil game_frames >= 3 20000 w2', 'run 60',
                                                      'poke dbg_uireq %d w1' % n, 'run %d' % wait] + extra + [shot(name)])


run_rom('wolf3d_dbg.sms', 'build_dbg/game.noi', [
    'run 200', 'waituntil game_frames >= 3 20000 w2', 'run 60', 'pause', 'run 100', shot('menu_game'),
    'tap down', 'run 12', 'tap down', 'run 12', 'tap down', 'run 12', 'tap 1', 'run 100', 'tap 1', 'run 150', shot('save')])
hooked(1, [], 'inter', 1000)
hooked(2, [], 'victory')
hooked(4, [], 'end')
hooked(6, ['tap right', 'run 60', 'tap right', 'run 60'], 'help')
hooked(3, ['tap up', 'run 15', 'tap right', 'run 15', 'tap up', 'run 15', 'tap up', 'run 15'], 'name')
hooked(7, [], 'scores')
order = ['pg13', 'title', 'credits', 'menu', 'sound', 'difficulty', 'psyched', 'menu_game', 'save', 'inter', 'victory', 'end',
         'help', 'name', 'scores']
W, H = 256, 192
sheet = Image.new('RGB', (W * 4 + 18, H * 4 + 18), (30, 30, 30))
for i, n in enumerate(order):
    im = Image.open(os.path.join(TMP, 'ui_%s.ppm' % n)).convert('RGB')
    sheet.paste(im, (6 + (i % 4) * (W + 2), 6 + (i // 4) * (H + 2)))
sheet.save(os.path.join(ROOT, 'docs', 'frontend.png'))
png('ui_title.ppm', 'title.png')
png('ui_menu.ppm', 'menu.png')
print('docs/*.png written')
