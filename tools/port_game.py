#!/usr/bin/env python3
"""One-off mechanical port of the Genesis game sources (wolf-3d-md/src/wl_*.c)
to the SMS layout (src/game/).  The output is then edited by hand; this
script is kept as a record and refuses to overwrite existing files unless -f.

 * SPEAR code is dropped, the shareware (UPLOAD) variant is kept
 * code of enemies that are not in episode 1 is removed from wl_act2.c
 * state tables move to gen/states.c (gen_states.py)
 * every global function becomes __banked (B)
"""
import os
import re
import sys

import gen_states

HERE = os.path.dirname(os.path.abspath(__file__))
import paths
ORIG = paths.MD
OUT = os.path.join(HERE, '..', 'src', 'game')
GEN = os.path.join(HERE, '..', 'gen')

# wl_game.c and wl_main.c are written by hand (no port): level setup from the
# ROM blobs, the SMS screens
FILES = ['wl_act1.c', 'wl_act2.c', 'wl_agent.c', 'wl_state.c', 'wl_play.c']

DEFINED = {'UPLOAD'}
UNDEFINED = {'SPEAR'}

REMOVE_FUNCS = '''A_Smoke A_Slurpie A_HitlerMorph A_MechaSound ProjectileTryMove T_Projectile SpawnGretel
T_Launch T_Will SpawnTrans T_UShoot SpawnUber SpawnWill SpawnDeath A_Relaunch A_Victory A_StartAttack
A_Breathing SpawnAngel T_SpectreWait A_Dormant SpawnSpectre SpawnGhosts SpawnSchabbs SpawnGift SpawnFat
T_SchabbThrow T_GiftThrow T_Schabb T_Gift T_Fat SpawnFakeHitler SpawnHitler T_FakeFire T_Fake T_Ghosts'''.split()
KEEP_STATES = re.compile(r'^s_(grd|dog|ss|boss|bj|player|attack|deathcam)')


def preprocess(src):
    """resolve #ifdef/#ifndef/#else/#endif on SPEAR and UPLOAD"""
    out = []
    stack = []          # (active_before, cond_value or None, in_else)
    active = True
    for line in src.split('\n'):
        m = re.match(r'\s*#\s*(ifdef|ifndef)\s+(\w+)\s*$', line)
        if m and (m.group(2) in DEFINED or m.group(2) in UNDEFINED):
            val = m.group(2) in DEFINED
            if m.group(1) == 'ifndef':
                val = not val
            stack.append((active, val))
            active = active and val
            continue
        m = re.match(r'\s*#\s*if\s', line)
        if m:
            stack.append((active, None))
            if active:
                out.append(line)
            continue
        if re.match(r'\s*#\s*else\b', line):
            if stack:
                prev, val = stack[-1]
                if val is None:
                    if active:
                        out.append(line)
                else:
                    stack[-1] = (prev, not val)
                    active = prev and not val
                    continue
            continue
        if re.match(r'\s*#\s*endif\b', line):
            prev, val = stack.pop()
            active = prev
            if val is None and active:
                out.append(line)
            continue
        if active:
            out.append(line)
    return '\n'.join(out)


def remove_func(src, name):
    m = re.search(r'^[A-Za-z_][^\n;{}()]*?\b%s\s*\([^;{}]*\)\s*\n\{' % re.escape(name), src, re.M)
    if not m:
        return src
    i = m.end() - 1
    depth = 0
    j = i
    while True:
        c = src[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                break
        j += 1
    return src[:m.start()] + src[j + 1:]


FUNC_RE = re.compile(r'^(?!static\b)(?!typedef\b)(?!extern\b)(?!return\b)(?!else\b)'
                     r'((?:const\s+)?[A-Za-z_]\w*(?:\s+\w+)*[\s\*]+)(\w+)\s*\(([^;{}()]*)\)\s*(;?)\s*$')


def bank_funcs(src):
    out = []
    for line in src.split('\n'):
        m = FUNC_RE.match(line)
        if m and m.group(2) not in ('if', 'while', 'for', 'switch', 'sizeof'):
            ret, name, args, semi = m.groups()
            line = '%s%s (%s) B%s' % (ret, name, args, semi)
        out.append(line)
    return '\n'.join(out)


def load(f):
    src = open(os.path.join(ORIG, f), encoding='latin1').read().replace('\r\n', '\n')
    src = preprocess(src)
    src = re.sub(r'\bsintable\[([^\]]+)\]', r'fsin(\1)', src)
    src = re.sub(r'\bcostable\[([^\]]+)\]', r'fcos(\1)', src)
    if f == 'wl_act2.c':
        for n in REMOVE_FUNCS:
            src = remove_func(src, n)
        # states of the other enemies
        def keep(m):
            return m.group(0) if KEEP_STATES.match(m.group(1)) else ''
        src = gen_states.STATE_RE.sub(keep, src)
    return src


def main():
    force = '-f' in sys.argv
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(GEN, exist_ok=True)
    srcs = {f: load(f) for f in FILES}
    # state table from the trimmed sources
    tmp = os.path.join(GEN, 'cache')
    os.makedirs(tmp, exist_ok=True)
    for f, s in srcs.items():
        open(os.path.join(tmp, f), 'w').write(s)
    gen_states.main(tmp, GEN, ['wl_act2.c', 'wl_agent.c'])
    for f, s in srcs.items():
        dst = os.path.join(OUT, f)
        if os.path.exists(dst) and not force:
            print('exists, skipped', dst)
            continue
        s = re.sub(r'^#include\s+"[^"]*"\s*\n', '', s, flags=re.M)
        s = gen_states.strip_states(s)
        s = bank_funcs(s)
        s = '#include "def.h"\n' + s
        open(dst, 'w').write(s)
        print('wrote', dst, len(s.split('\n')), 'lines')


if __name__ == '__main__':
    main()
