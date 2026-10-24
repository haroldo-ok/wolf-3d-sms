#!/usr/bin/env python3
"""Benchmarks on the debug ROM (real time, no determinism): fps of the scenarios of tests/bench/*.txt.

  python3 tools/bench.py [name ...]     (needs wolf3d_dbg.sms: make DEBUG=1)

fps = game frames / seconds of the profiled window (NTSC: 60 console frames per second).  Each script
also writes a call-graph dump (/tmp/s/cg_<name>.txt) for tools/prof_wolf.py."""
import os, re, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
names = sys.argv[1:] or ['walk', 'turn', 'room']
os.makedirs('/tmp/s', exist_ok=True)
for n in names:
    out = subprocess.run(['bash', 'tools/st.sh', 'wolf3d_dbg.sms', 'tests/bench/%s.txt' % n], cwd=ROOT,
                         capture_output=True, text=True, timeout=900).stdout
    p = re.findall(r'print: game_frames = (\d+) \(frame (\d+)\)', out)
    if len(p) < 2:
        print('%-6s no result\n%s' % (n, out[-400:]))
        continue
    (g0, f0), (g1, f1) = [(int(a), int(b)) for a, b in p[:2]]
    frames = g1 - g0
    secs = (f1 - f0) / 60.0
    print('%-6s %3d game frames in %5.1f s = %4.1f fps  (%4.1f console frames per game frame)' %
          (n, frames, secs, frames / secs, (f1 - f0) / max(frames, 1)))
