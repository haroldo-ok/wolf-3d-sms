#!/usr/bin/env python3
"""Equivalence test: runs the scenarios of tests/det/scenarios.py on wolf3d_det.sms (make DET=1) and compares,
frame by frame, the screen hash and, at the checkpoints, the game state hash with tests/det/golden.json.

  python3 tests/det/det.py            compare with the golden file (exit status 1 on any difference)
  python3 tests/det/det.py --update   record the golden file from this build (only after checking the
                                      difference is intended!)
  python3 tests/det/det.py idle walk  only these scenarios
  --rom FILE --noi FILE               another build (e.g. the one before an optimisation)
  --logic                             compare only the game state hashes and the final values (not the pictures),
                                      e.g. after a change of the view's size
"""
import json, os, re, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
from scenarios import SCENARIOS

GOLDEN = os.path.join(HERE, 'golden.json')
TMP = '/tmp/s'


def expand(items):
    """('tap', X, K) -> press / release pairs of one frame each, from the current frame"""
    out, cur = [], 0
    for it in items:
        if it[0] == 'until':
            cur = it[1]
        if it[0] == 'tap':
            for _ in range(it[2]):
                out += [('hold', it[1]), ('until', cur + 1), ('hold', 'none'), ('until', cur + 2)]
                cur += 2
        else:
            out.append(it)
    return out


def script(name, items, noi):
    # every scenario starts at game frame 1 (never at a console frame count: the first frames of a build may
    # take any time, e.g. the self checks of dbg.c)
    s = ['symbols ' + noi, 'hooklog ThreeDRefresh %s/det_%s.log' % (TMP, name), 'run 200',
         'waituntil game_frames >= 1 20000 w2']
    for it in expand(items):
        k = it[0]
        if k == 'until':
            s += ['waituntil game_frames >= %d 20000 w2' % it[1], 'print dbg_hash w2']
        elif k == 'hold':
            s.append('hold ' + it[1])
        elif k == 'poke':
            s.append('poke %s %d w1' % (it[1], it[2]))
        elif k == 'warp':
            s += ['poke dbg_wx %d w1' % it[1], 'poke dbg_wy %d w1' % it[2], 'poke dbg_wdir %d w1' % it[3], 'poke dbg_warp 1 w1']
        elif k == 'map':
            s.append('poke dbg_map %d w1' % it[1])
        else:
            raise ValueError(k)
    s += ['print dbg_tx w1', 'print dbg_ty w1', 'print dbg_hp w2', 'print dbg_score w2', 'print dbg_kills w2', 'print dbg_ammo w2',
          'hookstop']
    return '\n'.join(s) + '\n'


ROM = 'wolf3d_det.sms'
NOI = 'build_det/game.noi'


def bcall_addr(noi):
    for l in open(os.path.join(ROOT, noi)):
        m = re.match(r'DEF\s+___sdcc_bcall_ehl\s+(0x[0-9A-Fa-f]+)', l)
        if m:
            return m.group(1)
    raise SystemExit('no ___sdcc_bcall_ehl in ' + noi)


def run(name, items):
    os.makedirs(TMP, exist_ok=True)
    sp = os.path.join(TMP, 'det_%s.txt' % name)
    open(sp, 'w').write(script(name, items, NOI))
    # no mid-frame palette splits in the hashed pictures; banked call attribution is not needed
    env = dict(os.environ, SMSTEST_FLATPAL='1', SMSTEST_BCALL=bcall_addr(NOI))
    out = subprocess.run([os.path.join(ROOT, 'tools/smstest/smstest'), ROM, sp], cwd=ROOT, capture_output=True, text=True,
                         timeout=1500, env=env).stdout
    if 'RESULT: ALL PASSED' not in out:
        raise SystemExit('scenario %s did not run clean:\n%s' % (name, out[-800:]))
    state = []
    final = {}
    for m in re.finditer(r'print: (\w+) = (-?\d+) \(frame (\d+)\)', out):
        if m.group(1) == 'dbg_hash':
            state.append(int(m.group(2)))
        else:
            final[m.group(1)] = int(m.group(2))
    screens = [l.split()[1] for l in open(os.path.join(TMP, 'det_%s.log' % name)) if l.strip()]
    return {'state': state, 'final': final, 'screens': screens}


def main():
    global ROM, NOI
    args = sys.argv[1:]
    for opt in ('--rom', '--noi'):
        if opt in args:
            i = args.index(opt)
            if opt == '--rom':
                ROM = args[i + 1]
            else:
                NOI = args[i + 1]
            del args[i:i + 2]
    update = '--update' in args
    logic = '--logic' in args
    names = [a for a in args if not a.startswith('--')] or list(SCENARIOS)
    golden = json.load(open(GOLDEN)) if os.path.exists(GOLDEN) and not update else {}
    if update and os.path.exists(GOLDEN) and names != list(SCENARIOS):
        golden = json.load(open(GOLDEN))
    bad = 0
    for n in names:
        r = run(n, SCENARIOS[n])
        if update:
            golden[n] = r
            print('%-8s recorded: %d frames, final %s' % (n, len(r['screens']), r['final']))
            continue
        g = golden.get(n)
        if g is None:
            print('FAIL %s: not in the golden file' % n)
            bad += 1
            continue
        why = None
        if r['state'] != g['state']:
            i = next((i for i, (a, b) in enumerate(zip(r['state'], g['state'])) if a != b), min(len(r['state']), len(g['state'])))
            why = 'game state differs at checkpoint %d' % i
        elif not logic and r['screens'] != g['screens']:
            i = next((i for i, (a, b) in enumerate(zip(r['screens'], g['screens'])) if a != b), min(len(r['screens']), len(g['screens'])))
            why = 'screen differs at game frame %d (of %d/%d)' % (i, len(r['screens']), len(g['screens']))
        elif r['final'] != g['final']:
            why = 'final values %s != %s' % (r['final'], g['final'])
        if why:
            print('FAIL %s: %s' % (n, why))
            bad += 1
        else:
            print('PASS %s (%d frames, final %s)' % (n, len(r['screens']), r['final']))
    if update:
        json.dump(golden, open(GOLDEN, 'w'), indent=0)
        print('wrote', GOLDEN)
    sys.exit(1 if bad else 0)


main()
