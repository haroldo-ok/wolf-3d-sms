"""Names and indexes from the original headers (AUDIOWL6.H, GFXV_WL1.H)."""
import re, os

import paths
SRC = paths.SRC


def parse_enum_block(text, start_value=0):
    out = []
    v = start_value
    for line in text.split('\n'):
        line = re.sub(r'//.*', '', line).strip()
        if not line or line.startswith('typedef') or line.startswith('}'):
            continue
        for part in line.split(','):
            part = part.strip()
            if not part:
                continue
            if '=' in part:
                n, val = part.split('=')
                v = int(val.strip(), 0)
                out.append((n.strip(), v))
            else:
                out.append((part, v))
            v += 1
    return out


def audio_names():
    t = open(os.path.join(SRC, 'WOLFSRC', 'AUDIOWL6.H'), encoding='latin1').read()
    blocks = re.findall(r'typedef enum \{(.*?)\}\s*(\w+)\s*;', t, re.S)
    sounds = music = None
    for body, name in blocks:
        if 'HITWALLSND' in body:
            sounds = parse_enum_block(body)
        if '_MUS' in body:
            music = parse_enum_block(body)
    return sounds, music


def gfx_names():
    """pictures of the shareware v1.4 VGAGRAPH: the repo's WL1 header plus 8
    pictures (as the Genesis converter found out); -> [(name, chunk)]"""
    t = open(os.path.join(SRC, 'WOLFSRC', 'GFXV_WL1.H'), encoding='latin1').read()
    e = t[t.index('typedef enum'):t.index('}', t.index('typedef enum'))]
    names = ['H_BJPIC'] + re.findall(r'^\s*([A-Z0-9_]+),', e, re.M)
    names = [n for n in names if n not in ('ORDERSCREEN', 'ERRORSCREEN')]
    out = []
    for n in names:
        if n == 'C_OPTIONSPIC':
            out.append('H_APOGEEPIC')
        out.append(n)
        if n == 'C_CODEPIC':
            out += ['C_CODE2PIC', 'C_LEVELPIC', 'C_NAMEPIC', 'C_SCOREPIC', 'C_JOY1PIC', 'C_JOY2PIC']
        if n == 'L_EXPOINTPIC':
            out.append('L_APOSTROPHEPIC')
    assert len(out) == 144, len(out)
    return [(n, 3 + i) for i, n in enumerate(out)]


if __name__ == '__main__':
    s, m = audio_names()
    print(len(s), s[:3], s[-3:])
    print(len(m), m[:3], m[-3:])
    g = gfx_names()
    print(len(g), g[:3], g[-5:])
