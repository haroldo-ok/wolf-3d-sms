"""Sound and music -> SN76489 streams.

AdLib effects (AUDIOT 87+n) and the IMF songs (AUDIOT 261+n) become event streams for the player in src/snd.c:

  0x00 lo hi    end: lo hi = 0: stop;  else jump back by lo + 256 * hi bytes (from the 0x00) -- the song loops
  1..0x7F       wait that many ticks (70 per second)
  0x80..0xFF    voice event: bit 7 set, bits 6-5 channel (0-2 tone, 3 noise), bit 4 a period follows,
                bits 3-0 attenuation (0 loud, 15 off).  Period: 2 bytes (the 10 bit PSG period, low byte first);
                noise: 1 byte (the noise control 0..7: bit 2 white noise, bits 1-0 shift rate)

The digitised voices of VSWAP become 7.86 kHz samples of two bytes: the volume commands of PSG channels 1 and 2
(0xB0 | att, 0xD0 | att), summing two channels to get more than 16 levels; written as they are by the line interrupt.
"""
import struct, math, os
import numpy as np
import wl_assets, wl_names

PSG_CLK = 3579545
MULT = [0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15]
OPS = [0, 1, 2, 8, 9, 10, 16, 17, 18]            # modulator register offset of each OPL channel


def period_of(f):
    """PSG tone period for a frequency; notes below 109 Hz go up whole octaves"""
    if f <= 0:
        return 1023
    n = PSG_CLK / (32.0 * f)
    while n > 1023:
        n /= 2
    return max(1, min(1023, int(round(n))))


# ------------------------------------------------------------- envelopes
def t_attack(r):
    if r == 0:
        return 1e9
    if r >= 15:
        return 0.0
    return 0.0045 * 2 ** (10 - r)


def rate_dbps(r):
    """decay and release speed in dB per second"""
    if r == 0:
        return 0.0
    return 96.0 / (39.28 / 2 ** (r - 1))


def env_db(age, ar, dr, sl, rr, egt):
    """attenuation (dB) of an operator `age` seconds after key on"""
    ta = t_attack(ar)
    if age < ta:
        return 96.0 * (1 - age / ta)
    a2 = age - ta
    sl_db = 93.0 if sl == 15 else sl * 3.0
    rd = rate_dbps(dr)
    if rd > 0 and a2 * rd < sl_db:
        return a2 * rd
    t_sl = sl_db / rd if rd > 0 else 1e9
    if rd == 0:
        return 0.0
    if egt:
        return sl_db
    return min(96.0, sl_db + (a2 - t_sl) * rate_dbps(max(rr, 1)))


def att_of(db):
    return int(min(15, max(0, round(db / 2.0))))


# ------------------------------------------------------------------ SFX
def classify(data, inst):
    nz = [x for x in data if x]
    if not nz:
        return False
    jit = sum(abs(data[i + 1] - data[i]) for i in range(len(data) - 1)) / max(1, len(data) - 1)
    mean = sum(nz) / len(nz)
    conn = inst[10] & 1
    mtl = inst[2] & 63
    return (conn == 0 and mtl <= 12) or (jit > 28 and jit / mean > 0.25)


def noise_ctl(f):
    """white noise, shift rate from the pitch"""
    if f > 1500:
        return 4
    if f > 500:
        return 5
    return 6


class Events:
    """(tick, [bytes]) batches -> a stream"""
    def __init__(self):
        self.last = {}
        self.batches = []

    def at(self, tick, ev):
        if ev:
            self.batches.append((tick, ev))

    def stream(self, total_ticks, loop):
        out = bytearray()
        for i, (t, ev) in enumerate(self.batches):
            if i == 0 and t > 0:
                out += wait_bytes(t)
            out += ev
            nt = self.batches[i + 1][0] if i + 1 < len(self.batches) else total_ticks
            out += wait_bytes(nt - t)
        start = 0
        if loop:
            off = len(out)
            out += bytes([0, off & 0xFF, off >> 8])
        else:
            out += bytes([0, 0, 0])
        return bytes(out)


def wait_bytes(n):
    out = bytearray()
    while n > 0:
        k = min(127, n)
        out.append(k)
        n -= k
    return bytes(out)


def tone_ev(ch, per, att, period_changed=True):
    if period_changed:
        return bytes([0x80 | (ch << 5) | 0x10 | att, per & 0xFF, per >> 8])
    return bytes([0x80 | (ch << 5) | att])


def noise_ev(ctl, att, ctl_changed=True):
    if ctl_changed:
        return bytes([0x80 | (3 << 5) | 0x10 | att, ctl])
    return bytes([0x80 | (3 << 5) | att])


def sfx_stream(chunk):
    """AdLib sound chunk -> (stream, priority, noisy)"""
    length, pri = struct.unpack_from('<IH', chunk, 0)
    inst = chunk[6:22]
    block = chunk[22]
    data = list(chunk[23:23 + length])
    c_char, m_char = inst[1], inst[0]
    c_tl = inst[3] & 63
    c_ar, c_dr = inst[5] >> 4, inst[5] & 15
    c_sl, c_rr = inst[7] >> 4, inst[7] & 15
    egt = (c_char >> 5) & 1
    cmult = MULT[c_char & 15]
    noisy = classify(data, inst)
    # 140 Hz data bytes -> 70 Hz ticks (the louder of each pair, the later frequency)
    n = len(data)
    tail = 12
    ticks = (n + 1) // 2 + tail
    ev = Events()
    last = {'t': None, 'n': None}
    keyon_at = None
    off_db = None
    cur_att, cur_per, cur_nc = 15, None, None
    state_on = False
    t_on = 0.0
    t_off = None
    for k in range(ticks):
        i0, i1 = 2 * k, 2 * k + 1
        pair = [data[i] for i in (i0, i1) if i < n]
        s = max(pair) if pair else 0
        t = k / 70.0
        if s and (not state_on):
            state_on, t_on, t_off = True, t, None
        if (not s) and state_on:
            off_db = env_db(t - t_on, c_ar, c_dr, c_sl, c_rr, egt)
            state_on, t_off = False, t
        if state_on:
            db = env_db(t - t_on, c_ar, c_dr, c_sl, c_rr, egt) + c_tl * 0.75
        elif t_off is not None:
            db = off_db + (t - t_off) * rate_dbps(max(c_rr, 1)) + c_tl * 0.75
        else:
            db = 99.0
        att = att_of(db) if db < 90 else 15
        if state_on and s:
            f = s * 49716.0 / 2 ** (20 - block) * cmult
            per = period_of(f)
            nc = noise_ctl(f)
        else:
            per, nc = cur_per, cur_nc
        evs = bytearray()
        if noisy:
            if att != cur_att or nc != cur_nc:
                evs += noise_ev(nc if nc is not None else 5, att, nc != cur_nc)
                cur_nc = nc
        else:
            if att != cur_att or per != cur_per:
                evs += tone_ev(2, per if per is not None else 100, att, per != cur_per)
                cur_per = per
        cur_att = att
        ev.at(k, bytes(evs))
    # silence at the end
    evs = noise_ev(5, 15, False) if noisy else tone_ev(2, 0, 15, False)
    ev.at(ticks, evs if cur_att != 15 else b'')
    return ev.stream(ticks + 1, False), pri, noisy


# ----------------------------------------------------------------- music
def parse_imf(chunk):
    n = struct.unpack_from('<H', chunk, 0)[0]
    body = chunk[2:2 + n]
    evs, t = [], 0
    for i in range(0, len(body) - 3, 4):
        reg, val, dl = struct.unpack_from('<BBH', body, i)
        evs.append((t, reg, val))
        t += dl
    return evs, t


def song_stream(chunk, name=''):
    evs, total = parse_imf(chunk)
    nticks = (total * 70 + 699) // 700 + 1
    regs = [0] * 256
    kon = [False] * 9
    kon_t = [0.0] * 9
    off_t = [None] * 9
    off_db = [0.0] * 9
    note_id = [0] * 9
    rhythm = {'seen': 0}
    # pass 1: per tick channel states
    ei = 0
    states = []         # per tick: {ch: (period, att, note_id)}
    importance = [0.0] * 9
    for tick in range(nticks):
        tms = tick * 10                       # IMF ticks (700 Hz)
        while ei < len(evs) and evs[ei][0] <= tms:
            _, reg, val = evs[ei]
            ei += 1
            if 0xB0 <= reg <= 0xB8:
                c = reg - 0xB0
                on = bool(val & 0x20)
                tt = tick / 70.0
                if on and not kon[c]:
                    kon[c] = True
                    kon_t[c] = tt
                    off_t[c] = None
                    note_id[c] += 1
                elif (not on) and kon[c]:
                    co = OPS[c] + 3
                    ar, dr = regs[0x60 + co] >> 4, regs[0x60 + co] & 15
                    sl, rr = regs[0x80 + co] >> 4, regs[0x80 + co] & 15
                    egt = (regs[0x20 + co] >> 5) & 1
                    off_db[c] = env_db(tt - kon_t[c], ar, dr, sl, rr, egt)
                    kon[c] = False
                    off_t[c] = tt
            if reg == 0xBD and (val & 0x20):
                rhythm['seen'] += 1
            regs[reg] = val
        cur = {}
        tt = tick / 70.0
        for c in range(9):
            mo, co = OPS[c], OPS[c] + 3
            fnum = regs[0xA0 + c] | ((regs[0xB0 + c] & 3) << 8)
            block = (regs[0xB0 + c] >> 2) & 7
            conn = regs[0xC0 + c] & 1
            ctl = regs[0x40 + co] & 63
            mtl = regs[0x40 + mo] & 63
            ar, dr = regs[0x60 + co] >> 4, regs[0x60 + co] & 15
            sl, rr = regs[0x80 + co] >> 4, regs[0x80 + co] & 15
            egt = (regs[0x20 + co] >> 5) & 1
            if kon[c]:
                db = env_db(tt - kon_t[c], ar, dr, sl, rr, egt) + ctl * 0.75
            elif off_t[c] is not None:
                db = off_db[c] + (tt - off_t[c]) * rate_dbps(max(rr, 1)) + ctl * 0.75
            else:
                continue
            if conn:
                mdb = env_db(tt - kon_t[c], regs[0x60 + mo] >> 4, regs[0x60 + mo] & 15, regs[0x80 + mo] >> 4,
                             regs[0x80 + mo] & 15, (regs[0x20 + mo] >> 5) & 1) + mtl * 0.75 if kon[c] else 99
                db = min(db, mdb)
            att = att_of(db) if db < 90 else 15
            if att >= 15 or fnum == 0:
                continue
            f = fnum * 49716.0 / 2 ** (20 - block) * MULT[regs[0x20 + co] & 15]
            cur[c] = (period_of(f), att, note_id[c])
            importance[c] += (15 - att) / 15.0
        states.append(cur)
    # voice allocation: 3 PSG tone voices
    rank = sorted(range(9), key=lambda c: -importance[c])
    prio = {c: 9 - i for i, c in enumerate(rank)}
    voices = [None, None, None]                 # (chan, note id)
    last = [(None, 15)] * 3
    ev = Events()
    prev_att = [15, 15, 15]
    for tick, cur in enumerate(states):
        # free voices whose note is over
        for v in range(3):
            if voices[v] is not None:
                c, nid = voices[v]
                if c not in cur or cur[c][2] != nid:
                    voices[v] = None
        held = {voices[v][0] for v in range(3) if voices[v] is not None}
        newc = sorted([c for c in cur if c not in held], key=lambda c: -(prio[c] * 2 + (15 - cur[c][1]) * 0.3))
        for c in newc:
            free = [v for v in range(3) if voices[v] is None]
            if free:
                voices[free[0]] = (c, cur[c][2])
                continue
            # steal the weakest voice if this note is clearly more important
            w = min(range(3), key=lambda v: prio[voices[v][0]] * 2 + (15 - cur[voices[v][0]][1]) * 0.3)
            wc = voices[w][0]
            if prio[c] * 2 + (15 - cur[c][1]) * 0.3 > prio[wc] * 2 + (15 - cur[wc][1]) * 0.3 + 2:
                voices[w] = (c, cur[c][2])
        evs = bytearray()
        for v in range(3):
            if voices[v] is None:
                per, att = last[v][0], 15
            else:
                per, att, nid = cur[voices[v][0]]
                if tick % 2 and last[v][1] != 15 and abs(att - last[v][1]) <= 1 and per == last[v][0]:
                    att = last[v][1]            # envelope steps at 35 Hz
            if (per, att) != last[v]:
                if att == 15 and last[v][1] != 15:
                    evs += tone_ev(v, 0, 15, False)
                elif per is not None:
                    evs += tone_ev(v, per, att, per != last[v][0])
                last[v] = (per, att)
        ev.at(tick, bytes(evs))
    silence = bytearray()
    for v in range(3):
        if last[v][1] != 15:
            silence += tone_ev(v, 0, 15, False)
    ev.at(len(states), bytes(silence))
    return ev.stream(len(states) + 1, True), len(states), importance, rhythm['seen']


# --------------------------------------------------------------- digitised
ATT = np.array([10 ** (-2.0 * k / 20.0) for k in range(15)] + [0.0])
PAIRS = sorted({(round(float(ATT[a] + ATT[b]), 6), a, b) for a in range(16) for b in range(a, 16)})
PAIR_SUM = np.array([p[0] for p in PAIRS])


def pcm_of(raw):
    """8 bit 7 kHz -> 7.86 kHz samples of two bytes: the volume commands of PSG channels 1 and 2"""
    x = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128.0) / 128.0
    rate_in, rate_out = 7000.0, 7860.0
    n = int(len(x) * rate_out / rate_in)
    t = np.arange(n) * rate_in / rate_out
    y = np.interp(t, np.arange(len(x)), x)
    peak = max(1e-6, np.abs(y).max())
    y = y / peak
    out = bytearray()
    err = 0.0
    c, g = 0.62, 0.55
    for v in y:
        want = c + g * v + err
        i = int(np.searchsorted(PAIR_SUM, want))
        i = min(max(i, 1), len(PAIRS) - 1)
        j = i if abs(PAIR_SUM[i] - want) < abs(PAIR_SUM[i - 1] - want) else i - 1
        got = PAIR_SUM[j]
        err = (want - got) * 0.8
        err = max(-0.15, min(0.15, err))
        _, a, b = PAIRS[j]
        out += bytes([0xB0 | a, 0xD0 | b])
    return bytes(out)


FRAME = 192                  # bytes of a frame of 96 samples: what one video frame of line interrupts plays


def build(pk, H, C, d, cached):
    snd, mus = wl_names.audio_names()
    names = {v: k for k, v in snd}
    gen = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gen')
    # ---- effects
    sfx_bank, sfx_addr, sfx_pri = [], [], []
    nbytes = 0
    for n in range(87):
        s, pri, noisy = sfx_stream(d.audio_chunk(87 + n))
        b, a = pk.add('sfx%d' % n, s)
        sfx_bank.append(b); sfx_addr.append(a); sfx_pri.append(min(pri, 255))
        nbytes += len(s)
    # ---- songs
    mus_bank, mus_addr = [], []
    mbytes = 0
    for i in range(27):
        key = 'song%d' % i
        s = cached(key, lambda i=i: song_stream(d.audio_chunk(261 + i))[0])
        b, a = pk.add(key, s)
        mus_bank.append(b); mus_addr.append(a)
        mbytes += len(s)
    # ---- digitised voices: consecutive banks, 96 byte frames that never straddle a bank
    digimap = {'HALTSND': 0, 'DOGBARKSND': 1, 'CLOSEDOORSND': 2, 'OPENDOORSND': 3, 'ATKMACHINEGUNSND': 4,
               'ATKPISTOLSND': 5, 'ATKGATLINGSND': 6, 'SCHUTZADSND': 7, 'GUTENTAGSND': 8, 'MUTTISND': 9,
               'BOSSFIRESND': 10, 'SSFIRESND': 11, 'DEATHSCREAM1SND': 12, 'DEATHSCREAM2SND': 13,
               'DEATHSCREAM3SND': 13, 'TAKEDAMAGESND': 14, 'PUSHWALLSND': 15}
    pcm = []
    for i in range(16):
        blob = cached('pcm%d' % i, lambda i=i: pcm_of(d.digi_sound(i)))
        blob += b'\xbf\xdf' * (((-len(blob)) % FRAME) // 2)
        pcm.append(blob)
    per_bank = 16384 // FRAME                      # 85 frames
    frames = []
    starts = []
    f = 0
    for blob in pcm:
        starts.append(f)
        f += len(blob) // FRAME
        frames.append(blob)
    allpcm = b''.join(frames)
    first = None
    for i in range(0, len(allpcm), per_bank * FRAME):
        b = pk.add_raw_bank('pcm%d' % (i // (per_bank * FRAME)), allpcm[i:i + per_bank * FRAME])
        if first is None:
            first = b
    pcm_len = [len(b) // FRAME for b in pcm]
    # voices (guards, screams, damage, the dog): the default; the weapons, doors and the push wall only in "all sounds"
    voice = [1 if i in (0, 1, 7, 8, 9, 12, 13, 14) else 0 for i in range(16)]
    digi_of = [255] * 88
    for name, idx in digimap.items():
        if name in dict(snd):
            digi_of[dict(snd)[name]] = idx
    H.append('#define PCM_FRAME %d' % FRAME)
    H.append('#define PCM_PER_BANK %d' % per_bank)
    H.append('#define PCM_BANK0 %d' % first)
    H.append('#define NUM_SFX 87')
    H.append('#define NUM_SONGS 27')
    with open(os.path.join(gen, 'snd_tab.c'), 'w') as fh:
        fh.write('/* generated by tools/wl_audio.py */\n')
        fh.write('const unsigned char sfx_bank[87] = {%s};\n' % ','.join(map(str, sfx_bank)))
        fh.write('const unsigned short sfx_addr[87] = {%s};\n' % ','.join('0x%04X' % a for a in sfx_addr))
        fh.write('const unsigned char sfx_pri[87] = {%s};\n' % ','.join(map(str, sfx_pri)))
        fh.write('const unsigned char mus_bank[27] = {%s};\n' % ','.join(map(str, mus_bank)))
        fh.write('const unsigned short mus_addr[27] = {%s};\n' % ','.join('0x%04X' % a for a in mus_addr))
        fh.write('const unsigned char digi_of[88] = {%s};\n' % ','.join(map(str, digi_of)))
        fh.write('const unsigned short pcm_start[16] = {%s};\n' % ','.join(map(str, starts)))
        fh.write('const unsigned short pcm_frames[16] = {%s};\n' % ','.join(map(str, pcm_len)))
        fh.write('const unsigned char digi_voice[16] = {%s};\n' % ','.join(map(str, voice)))
    print('audio: %d effect bytes, %d music bytes, %d pcm bytes in %d banks' % (nbytes, mbytes, len(allpcm), (len(allpcm) + per_bank * FRAME - 1) // (per_bank * FRAME)))


if __name__ == '__main__':
    import sys
    d = wl_assets.data()[0]
    snd, mus = wl_names.audio_names()
    tot = 0
    for i in range(27):
        s, nt, imp, rh = song_stream(d.audio_chunk(261 + i))
        tot += len(s)
        print('song %2d %-16s %6d bytes %5d ticks (%.0fs)  rhythm writes %d  importance %s' % (
            i, mus[i][0] if i < len(mus) else '?', len(s), nt, nt / 70.0, rh, ' '.join('%.0f' % x for x in imp)))
    print('total music', tot)
