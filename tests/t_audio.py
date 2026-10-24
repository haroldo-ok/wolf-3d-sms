#!/usr/bin/env python3
"""Audio checks on the PSG write log of smstest (psglog):
  1. the title screen plays its music: tone writes, several pitches on every voice
  2. a digitised voice (guard's HALT) plays as ~190 volume writes per frame, ends by itself, and the game goes on
  3. an effect (pistol, not a voice) is a burst of noise volume changes
exit status 0 = all passed"""
import os, re, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import psg2wav
TMP = '/tmp/s'
os.makedirs(TMP, exist_ok=True)
fails = []


def run(rom, script, noi):
    open(TMP + '/t_audio.txt', 'w').write('symbols %s\n%s\n' % (noi, script))
    env = dict(os.environ, SMSTEST_BCALL='0x' + re.search(r'^\s*([0-9A-Fa-f]+)\s+___sdcc_bcall_ehl', open(os.path.join(ROOT, os.path.dirname(noi), 'game.map')).read(), re.M).group(1).lstrip('0'))
    out = subprocess.run([os.path.join(ROOT, 'tools/smstest/smstest'), os.path.join(ROOT, rom), TMP + '/t_audio.txt'], cwd=ROOT,
                         capture_output=True, text=True, env=env).stdout
    return out


def check(ok, msg):
    print(('PASS ' if ok else 'FAIL ') + msg)
    if not ok:
        fails.append(msg)


# 1. the title music
run('wolf3d.sms', 'psglog %s/ta1.log\nrun 900\npsgstop' % TMP, 'build/game.noi')
ev = psg2wav.parse(TMP + '/ta1.log')
w, notes = psg2wav.summary(ev)
check(w > 150 and all(len(n) >= 4 for n in notes), 'title music: %d writes, pitches per voice %s' % (w, [len(n) for n in notes]))

# 2. a voice: HALTSND (21) -> sound number + 1 = 22
out = run('wolf3d_dbg.sms', '''run 200
waituntil game_frames >= 3 20000 w2
run 40
poke dbg_sndreq 22 w1
run 4
print pcm_on w1
psglog %s/ta2.log
run 90
psgstop
print pcm_on w1
print game_frames w2
run 60
print game_frames w2''' % TMP, 'build_dbg/game.noi')
vals = [int(x) for x in re.findall(r'print: \w+ = (\d+)', out)]
check(vals[0] == 1 and vals[1] == 0, 'voice: playing (pcm_on %d), finished by itself (pcm_on %d)' % (vals[0], vals[1]))
check(vals[3] > vals[2], 'voice: the game goes on (game_frames %d -> %d)' % (vals[2], vals[3]))
frames = {}
for l in open(TMP + '/ta2.log'):
    f, b = l.split()
    frames[int(f)] = frames.get(int(f), 0) + 1
busy = sorted(frames.values())[-10:]
check(min(busy) >= 150, 'voice: %d..%d PSG writes in the busiest frames (96 samples of 2 bytes)' % (min(busy), max(busy)))

# 3. an effect: ATKPISTOLSND (24) -> 25; the pistol is not a voice: the PSG version, a noise burst
out = run('wolf3d_dbg.sms', '''run 200
waituntil game_frames >= 3 20000 w2
run 40
psglog %s/ta3.log
poke dbg_sndreq 25 w1
run 60
psgstop''' % TMP, 'build_dbg/game.noi')
ev = psg2wav.parse(TMP + '/ta3.log')
nv = [e[2][3] for e in ev]
changes = sum(1 for i in range(1, len(nv)) if nv[i] != nv[i - 1])
check(changes >= 6, 'effect: noise volume changed %d times (a burst with a decaying tail)' % changes)
sys.exit(1 if fails else 0)
