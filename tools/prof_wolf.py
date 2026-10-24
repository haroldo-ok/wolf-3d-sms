#!/usr/bin/env python3
"""prof_wolf.py - bank-aware report for an smstest call-graph dump (cgdump).

  python3 tools/prof_wolf.py dump.txt build_dbg [--frames N] [--top N] [--tree FUNC]

The stock prof2.py drops every symbol >= 0xC000, so all the banked game code (linked at
bank<<16 | 0x4000, e.g. _TryWalk = 0x34245) ends up in one catch-all bucket.  Here the
symbols come from the .noi and the relocated .rst listings of the build directory and keep
their bank, a program counter in 0x4000..0x7FFF with no bank prefix is bank 1, and code
running from slot 2 (0x8000..0xBFFF: the compiled scalers in the data banks) is shown as
"slot2 scaler code".
"""
import sys, re, bisect, collections, glob, os

args = sys.argv[1:]
opt = {}
for k in ('--frames', '--top', '--tree', '--callers', '--asm'):
    if k in args:
        i = args.index(k)
        opt[k] = args[i + 1]
        del args[i:i + 2]
if len(args) != 2:
    print(__doc__)
    sys.exit(2)
dump, bdir = args

syms = {}


def codekey(a):
    """a symbol/pc key -> normalised key, or None when it is not code"""
    if a < 0x4000:
        return a
    if a < 0x8000:
        return 0x10000 | a                      # bank 1
    if a >= 0x10000 and 0x4000 <= (a & 0xFFFF) < 0x8000:
        return a
    return None


for l in open(os.path.join(bdir, 'game.noi')):
    m = re.match(r'DEF\s+(\S+)\s+0x([0-9A-Fa-f]+)', l)
    if m:
        k = codekey(int(m.group(2), 16))
        n = m.group(1)
        if k is not None and not n.startswith(('l_', 's_', '.')) and '$' not in n:
            syms.setdefault(k, n.lstrip('_'))
lab = re.compile(r'^\s+([0-9A-F]{6})\s+\d+\s+([A-Za-z_][A-Za-z0-9_]*)::?')
for r in glob.glob(os.path.join(bdir, '*.rst')):
    for l in open(r, errors='replace'):
        m = lab.match(l)
        if m:
            k = codekey(int(m.group(1), 16))
            if k is not None:
                syms[k] = m.group(2).lstrip('_')
addrs = sorted(syms)
names = [syms[a] for a in addrs]


def owner(a):
    if a in (0x38, 0x66):
        return 'IRQ' if a == 0x38 else 'NMI'
    if a == 0xFFFFFF:
        return 'ROOT'
    if 0x8000 <= a < 0xC000:
        return 'slot2 code (scalers)'
    if a >= 0xC000 and a < 0x10000:
        return 'RAM code'
    k = codekey(a)
    if k is None:
        return '?%06X' % a
    i = bisect.bisect_right(addrs, k) - 1
    if i < 0 or (addrs[i] >> 16) != (k >> 16) and k >= 0x10000:
        return '?%06X' % a
    return names[i]


T = 0
ex = collections.Counter()
inc = []
edges = []
for l in open(dump):
    p = l.split()
    if p[0] == 'T':
        T = int(p[1])
    elif p[0] == 'X':
        ex[owner(int(p[1], 16))] += int(p[2])
    elif p[0] == 'I':
        inc.append((int(p[3]), int(p[2]), owner(int(p[1], 16)), int(p[1], 16)))
    elif p[0] == 'E':
        edges.append((int(p[1], 16), int(p[2], 16), int(p[3]), int(p[4])))
fr = int(opt.get('--frames', 0))
top = int(opt.get('--top', 40))


def fmt(c):
    return '%10d %5.1f%%' % (c, 100 * c / T) + (('  %8.0f/gf' % (c / fr)) if fr else '')


print('total %d cycles (%.0f console frames)%s' % (T, T / 59736, (', %d game frames -> %.0f cycles/game frame' % (fr, T / fr)) if fr else ''))
print('\n== exclusive (self) ==')
for n, c in ex.most_common(top):
    print('%-24s' % n, fmt(c))
print('\n== inclusive (entry -> return, ISR time excluded) ==')
seen = set()
for c, calls, n, a in sorted(inc, reverse=True):
    if len(seen) >= top:
        break
    if n in seen:
        continue
    seen.add(n)
    print('%-24s' % n, fmt(c), ' calls %7d  %7.0f cyc/call' % (calls, c / max(calls, 1)))
if '--tree' in opt:
    root = opt['--tree']
    kids = collections.defaultdict(list)
    for par, key, calls, incl in edges:
        kids[owner(par)].append((incl, calls, owner(key)))

    def walk(n, depth, maxd=4):
        for incl, calls, k in sorted(kids.get(n, []), reverse=True)[:8]:
            print('  ' * depth + '%-22s %9d (%4.1f%%) calls %6d' % (k, incl, 100 * incl / T, calls))
            if depth < maxd and k != n:
                walk(k, depth + 1, maxd)
    print('\n== call tree under %s ==' % root)
    walk(root, 1)

if '--callers' in opt:
    for fn in opt['--callers'].split(','):
        agg = collections.defaultdict(lambda: [0, 0])
        for par, key, calls, incl in edges:
            if owner(key) == fn:
                a = agg[owner(par)]
                a[0] += calls
                a[1] += incl
        print('\n== callers of %s ==' % fn)
        for k, (calls, incl) in sorted(agg.items(), key=lambda kv: -kv[1][1])[:10]:
            print('  %-24s calls %6d  incl %9d (%4.1f%%)%s' % (k, calls, incl, 100 * incl / T, ('  %8.0f/gf' % (incl / fr)) if fr else ''))

if '--asm' in opt:
    fn = opt['--asm']
    # instruction text of the listings, by (bank-qualified) address
    ins = re.compile(r'^\s+([0-9A-F]{6})\s+([0-9A-F ]{2,12}?)\s+\[\s*\d+\]\s+\d+\s+(.*)$')
    text = {}
    for r in glob.glob(os.path.join(bdir, '*.rst')):
        for l in open(r, errors='replace'):
            m = ins.match(l)
            if m:
                k = codekey(int(m.group(1), 16))
                if k is not None:
                    text[k] = m.group(3).strip()
    start = [a for a in addrs if syms[a] == fn]
    if not start:
        print('no function', fn)
    else:
        a0 = start[0]
        a1 = next((a for a in addrs if a > a0), a0 + 0x10000)
        rows = []
        for l in open(dump):
            p_ = l.split()
            if p_[0] == 'X':
                k = codekey(int(p_[1], 16)) if int(p_[1], 16) < 0x10000 else int(p_[1], 16)
                if k is not None and a0 <= k < a1:
                    rows.append((int(p_[2]), k))
        rows.sort(reverse=True)
        tot = sum(c for c, k in rows)
        print('\n== %s: %d cycles in the range (top instructions) ==' % (fn, tot))
        for c, k in rows[:top]:
            print('%8d %5.1f%%  %06X  %s' % (c, 100.0 * c / max(tot, 1), k, text.get(k, '?')))
