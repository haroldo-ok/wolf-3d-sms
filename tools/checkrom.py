import sys, re
mp, rom = sys.argv[1], sys.argv[2]
data_bank = int(sys.argv[3]) if len(sys.argv) > 3 else 8
txt = open(mp).read()
ends = {}
for name, addr, size in re.findall(r'^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})', txt, re.M):
    a = int(addr, 16); s = int(size, 16)
    ends[name] = (a, s)
bad = False
top0 = 0
for name in ('_CODE', '_INITIALIZER', '_GSINIT', '_GSFINAL', '_HOME', '_CONST', '_RODATA'):
    if name in ends:
        a, s = ends[name]
        top0 = max(top0, a + s)
        if a + s > 0x4000:
            print('ERROR: %s ends at %04X (> 0x4000, bank 0 overflow)' % (name, a + s)); bad = True
print('bank0 ends at %04X (%d bytes free)' % (top0, 0x4000 - top0))
for name in ('_DATA', '_INITIALIZED', '_BSS', '_HEAP'):
    if name in ends:
        a, s = ends[name]
        print('%s %04X-%04X' % (name, a, a + s))
        if name == '_INITIALIZED' and s:
            print('WARNING: initialised RAM data (%d bytes)' % s)
for name, (a, s) in sorted(ends.items()):
    m = re.match(r'_BANK(\d+)$', name)
    if m:
        b = int(m.group(1))
        end = (a & 0xFFFF) + s
        print('BANK%d %d bytes (%d free)' % (b, s, 0x8000 - end))
        if end > 0x8000:
            print('ERROR: BANK%d overflow' % b); bad = True
        if b >= data_bank:
            print('ERROR: BANK%d collides with the data banks' % b); bad = True
sys.exit(1 if bad else 0)
