#!/usr/bin/env python3
"""SN76489 write log of smstest (psglog: "frame byte" lines) -> register timeline summary and a WAV.

  python3 tools/psg2wav.py log.txt [out.wav] [--fps 60]
"""
import sys, wave
import numpy as np

CLK = 3579545.0


def parse(path):
    per, vol, noise = [1, 1, 1], [15, 15, 15, 15], 0
    latch = (0, 0)
    ev = []                                   # (frame, per0..2, vol0..3, noise)
    for line in open(path):
        f, b = line.split()
        f, b = int(f), int(b, 16)
        if b & 0x80:
            ch = (b >> 5) & 3
            t = (b >> 4) & 1
            d = b & 15
            latch = (ch, t)
            if t:
                vol[ch] = d
            elif ch < 3:
                per[ch] = (per[ch] & 0x3F0) | d
            else:
                noise = d
        else:
            ch, t = latch
            if not t and ch < 3:
                per[ch] = (per[ch] & 15) | ((b & 0x3F) << 4)
        ev.append((f, tuple(per), tuple(vol), noise))
    return ev


def render(ev, fps=60.0, rate=44100, out='out.wav'):
    last = ev[-1][0] + 1
    n = int(last / fps * rate)
    y = np.zeros(n, np.float64)
    amp = [10 ** (-2.0 * k / 20.0) if k < 15 else 0.0 for k in range(16)]
    phase = [0.0, 0.0, 0.0]
    lfsr = 0x8000
    ni = 0
    state = ((1, 1, 1), (15, 15, 15, 15), 0)
    bounds = [(e[0], e[1], e[2], e[3]) for e in ev]
    j = 0
    spf = rate / fps
    nz_acc = 0.0
    for fr in range(last):
        while j < len(bounds) and bounds[j][0] <= fr:
            state = (bounds[j][1], bounds[j][2], bounds[j][3])
            j += 1
        per, vol, nctl = state
        i0, i1 = int(fr * spf), min(n, int((fr + 1) * spf))
        t = np.arange(i0, i1)
        seg = np.zeros(i1 - i0)
        for c in range(3):
            if vol[c] < 15 and per[c] > 1:
                f = CLK / (32.0 * per[c])
                seg += amp[vol[c]] * np.where(((t * f / rate) % 1.0) < 0.5, 1.0, -1.0)
        if vol[3] < 15:
            rates = [CLK / 512, CLK / 1024, CLK / 2048, CLK / (32.0 * max(1, per[2]))]
            fn = rates[nctl & 3]
            steps = ((t * fn / rate).astype(np.int64))
            rng = np.random.RandomState(1)
            seg += amp[vol[3]] * (np.sign(np.sin(steps * 12.9898) * 43758.5453 % 1 - 0.5))
        y[i0:i1] = seg
    y = y / max(1.0, np.abs(y).max()) * 0.6
    with wave.open(out, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((y * 32767).astype(np.int16).tobytes())


def summary(ev, fps=60.0):
    notes = [set(), set(), set()]
    writes = len(ev)
    for f, per, vol, nz in ev:
        for c in range(3):
            if vol[c] < 15 and per[c] > 1:
                notes[c].add(round(CLK / (32.0 * per[c])))
    return writes, notes


if __name__ == '__main__':
    ev = parse(sys.argv[1])
    w, notes = summary(ev)
    print('%d PSG writes over %d frames' % (w, ev[-1][0] - ev[0][0]))
    for c in range(3):
        print(' channel %d: %d distinct pitches %s' % (c, len(notes[c]), sorted(notes[c])[:14]))
    if len(sys.argv) > 2 and sys.argv[2].endswith('.wav'):
        render(ev, out=sys.argv[2])
        print('wrote', sys.argv[2])
