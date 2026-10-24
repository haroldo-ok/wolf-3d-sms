"""All full screens of the front end (title, menus, scores, intermission, help ...) -> ROM banks.

  python3 wl_pics.py [name ...]     previews in /tmp/s/scr_<name>.png (3x), tile counts
"""
import sys, os
import numpy as np
import scrcore, wl_gfx, wl_scr, wl_text, sms
from wl_scr import *

SCREENS = []                # (name, function -> ('photo', idx, kwargs) | ('ui', ScreenDef))


def screen(fn):
    SCREENS.append((fn.__name__[2:], fn))
    return fn


def most_common(a):
    return int(np.bincount(a.reshape(-1)).argmax())


def with_cursor(sd):
    c1, c2 = wl_gfx.pic256('C_CURSOR1PIC'), wl_gfx.pic256('C_CURSOR2PIC')
    sd.cursor = (c1, c2)
    sd.cursor_bg = most_common(c1)
    return sd


# ------------------------------------------------------------------ photos
@screen
def s_pg13():
    cv = Canvas(wl_gfx.pic256('PG13PIC')[0, 0])
    cv.pic(160, 112, 'PG13PIC')
    return ('photo', cv.p, {})


@screen
def s_title():
    s, _, _ = scrcore.scale_screen(wl_gfx.pic256('TITLEPIC'))
    return ('photo', s, dict(fixed=[(2, 1, 0), (3, 2, 1)]))


@screen
def s_credits():
    s, _, _ = scrcore.scale_screen(wl_gfx.pic256('CREDITSPIC'))
    return ('photo', s, {})


# ------------------------------------------------------------------- menus
MAIN_ITEMS = ['New Game', 'Sound', 'Load Game', 'Save Game', 'Read This!', 'View Scores', None, 'Quit']


def main_menu(name, ingame):
    sd = with_cursor(ScreenDef(name))
    cv = sd.cv
    cv.pic(52, 0, 'C_OPTIONSPIC')
    cv.window(24, 52, 208, 136)
    for i, label in enumerate(MAIN_ITEMS):
        y = 56 + 16 * i
        flags = 0
        col = TEXT
        if label is None:
            label = 'Back to Game' if ingame else 'Back to Title'
        if label == 'Read This!':
            col = READH
        if label == 'Save Game' and not ingame:
            col, flags = BORD, 1
        cv.text(1, 56, y + 1, label, col)
        sd.item(7, y // 8, 22, 2, 28, y, flags)
    return ('ui', sd)


@screen
def s_main_title():
    return main_menu('main_title', False)


@screen
def s_main_game():
    return main_menu('main_game', True)


@screen
def s_newgame():
    sd = with_cursor(ScreenDef('newgame'))
    cv = sd.cv
    cv.text_c(1, 128, 20, 'How tough are you?', READH)
    cv.window(8, 48, 240, 80)
    for i, label in enumerate(["Can I play, Daddy?", "Don't hurt me.", "Bring 'em on!", "I am Death incarnate!"]):
        y = 56 + 16 * i
        cv.text(1, 40, y + 1, label, TEXT)
        sd.item(5, y // 8, 25, 2, 12, y)
    faces = [wl_gfx.pic256(n) for n in ('C_BABYMODEPIC', 'C_EASYPIC', 'C_NORMALPIC', 'C_HARDPIC')]
    cv.pic(116, 144, faces[0])
    sd.variant(14, 18, faces)
    return ('ui', sd)


@screen
def s_sound():
    sd = with_cursor(ScreenDef('sound'))
    cv = sd.cv
    groups = [('C_FXTITLEPIC', ['None', 'SN76489 PSG'], 2),
              ('C_DIGITITLEPIC', ['None', 'Voices only', 'All sounds'], 58),
              ('C_MUSICTITLEPIC', ['None', 'SN76489 PSG'], 130)]
    sel, nsel = wl_gfx.pic256('C_SELECTEDPIC'), wl_gfx.pic256('C_NOTSELECTEDPIC')
    for head, opts, top in groups:
        cv.pic(60, top, head)
        first = top + 22                    # first item: a multiple of 8
        first = (first + 7) // 8 * 8
        cv.window(16, top + 18, 224, 16 * len(opts) + 4)
        for k, label in enumerate(opts):
            y = first + 16 * k
            cv.text(1, 80, y + 1, label, TEXT)
            sd.item(10, y // 8, 20, 2, 20, y)
            cv.pic(48, y, nsel)
            sd.variant(6, y // 8, [nsel, sel])
    return ('ui', sd)


def confirm(name, lines):
    sd = with_cursor(ScreenDef(name))
    cv = sd.cv
    top = 24 + (3 - len(lines)) * 7
    cv.window(24, top, 208, 24 + 15 * len(lines) + 40)
    y = top + 12
    for s in lines:
        cv.text_c(1, 128, y, s, READH)
        y += 15
    y0 = (y + 8 + 7) // 8 * 8
    for i, label in enumerate(['Yes', 'No']):
        cv.text(1, 104, y0 + 16 * i + 1, label, TEXT)
        sd.item(12, y0 // 8 + 2 * i, 12, 2, 76, y0 + 16 * i)
    return ('ui', sd)


@screen
def s_conf_new():
    return confirm('conf_new', ['You are in a game.', 'Continuing will erase', 'the old game.  Ok?'])


@screen
def s_conf_quit():
    return confirm('conf_quit', ['Dost thou wish to leave', 'with such hasty abandon?'])



LAYOUT = {}                 # name -> value, emitted as #define LAY_<name> for the program


def field(prefix, col, row, w, h=2):
    LAYOUT[prefix + '_COL'] = col
    LAYOUT[prefix + '_ROW'] = row
    LAYOUT[prefix + '_W'] = w


# ------------------------------------------------------------ high scores
@screen
def s_hscore():
    sd = ScreenDef('hscore')
    sd.font_bg = BORD
    cv = sd.cv
    cv.pic(16, 0, 'HIGHSCORESPIC')
    cv.pic(16, 60, 'C_NAMEPIC')
    cv.pic(152, 60, 'C_LEVELPIC')
    cv.pic(216, 60, 'C_SCOREPIC')
    LAYOUT['HS_ROW0'] = 9           # first score line (a line every 2 cell rows), 7 lines
    LAYOUT['HS_NAME_COL'] = 2       # 15 characters
    LAYOUT['HS_LEVEL_COL'] = 19     # 2 characters
    LAYOUT['HS_SCORE_COL'] = 24     # 8 characters
    return ('ui', sd)


@screen
def s_name():
    sd = ScreenDef('name')
    sd.font_bg = BORD
    cv = sd.cv
    cv.text_c(1, 128, 24, 'New high score!', READH)
    cv.text_c(1, 128, 48, 'Enter your name', TEXT)
    cv.window(56, 76, 144, 20)
    cv.text_c(0, 128, 118, 'Up/Down: change letter', TEXT)
    cv.text_c(0, 128, 130, 'Right: next   Left: back', TEXT)
    cv.text_c(0, 128, 142, '1: done   2: delete', TEXT)
    LAYOUT['NAME_COL'] = 8
    LAYOUT['NAME_ROW'] = 10         # y = 80: inside the window
    LAYOUT['NAME_W'] = 15
    return ('ui', sd)


def slots_screen(name, banner):
    sd = with_cursor(ScreenDef(name))
    sd.font_bg = BKGD
    cv = sd.cv
    cv.pic(20, 0, banner)
    for k in range(3):
        y = 56 + 48 * k
        cv.window(8, y, 240, 36)
        sd.item(5, y // 8 + 1, 26, 2, 12, y + 8)
    LAYOUT['SLOT_ROW0'] = 8         # first text line of slot 0, a slot every 6 cell rows, 2 lines each
    LAYOUT['SLOT_COL'] = 5
    LAYOUT['SLOT_W'] = 26
    return ('ui', sd)


@screen
def s_load():
    return slots_screen('load', 'C_LOADGAMEPIC')


@screen
def s_save():
    return slots_screen('save', 'C_SAVEGAMEPIC')


# ------------------------------------------------------------ intermission
def inter_base(name):
    sd = ScreenDef(name, bg=VIEW)
    sd.fixed = [(0, 1, 1)]
    sd.slot_of = {VIEW: 0}
    sd.hi = {}
    sd.protect = ()
    sd.digits = VIEW
    return sd


@screen
def s_inter():
    sd = inter_base('inter')
    cv = sd.cv
    g1, g2 = wl_gfx.pic256('L_GUYPIC'), wl_gfx.pic256('L_GUY2PIC')
    cv.pic(8, 16, g1)
    sd.variant(1, 2, [g1, g2])
    label_text(cv, 120, 16, 'FLOOR')
    label_text(cv, 120, 32, 'COMPLETED')
    label_text(cv, 112, 56, 'BONUS', pitch=11)
    label_text(cv, 120, 80, 'TIME')
    label_text(cv, 120, 96, ' PAR')
    for y, t in ((112, 'KILL RATIO'), (128, 'SECRET RATIO'), (144, 'TREASURE RATIO')):
        label_text(cv, 8, y, t)
        cv.pic(232, y, 'L_PERCENTPIC')
    label_text(cv, 8, 168, 'SCORE')
    field('INTER_FLOOR', 23, 2, 4)
    field('INTER_BONUS', 22, 7, 10)
    field('INTER_TIME', 23, 10, 9)
    field('INTER_PAR', 23, 12, 9)
    field('INTER_KILL', 23, 14, 6)
    field('INTER_SECRET', 23, 16, 6)
    field('INTER_TREAS', 23, 18, 6)
    field('INTER_SCORE', 18, 21, 14)
    return ('ui', sd)


@screen
def s_inter2():
    sd = inter_base('inter2')
    cv = sd.cv
    g1, g2 = wl_gfx.pic256('L_GUYPIC'), wl_gfx.pic256('L_GUY2PIC')
    cv.pic(8, 16, g1)
    sd.variant(1, 2, [g1, g2])
    label_text(cv, 120, 16, 'SECRET FLOOR'[:9])
    label_text(cv, 120, 32, 'FLOOR')
    label_text(cv, 120, 56, 'COMPLETED!')
    label_text(cv, 20, 120, '15000 BONUS!')
    label_text(cv, 8, 168, 'SCORE')
    field('INTER2_SCORE', 18, 21, 14)
    return ('ui', sd)


@screen
def s_victory():
    sd = inter_base('victory')
    cv = sd.cv
    cv.pic(0, 24, 'L_BJWINSPIC')
    label_text(cv, 80, 4, 'YOU WIN!')
    label_text(cv, 92, 32, 'TOTAL TIME', pitch=11)
    label_text(cv, 92, 80, 'AVERAGES', pitch=11)
    for y, t in ((112, 'KILL'), (128, 'SECRET'), (144, 'TREASURE')):
        label_text(cv, 92, y, t, pitch=11)
        cv.pic(240, y, 'L_PERCENTPIC')
    field('VICT_TIME', 13, 7, 9)
    field('VICT_KILL', 24, 14, 6)
    field('VICT_SECRET', 24, 16, 6)
    field('VICT_TREAS', 24, 18, 6)
    return ('ui', sd)


@screen
def s_psyched():
    sd = ScreenDef('psyched')
    cv = sd.cv
    cv.pic(16, 40, 'GETPSYCHEDPIC')
    cv.window(60, 112, 136, 16)
    empty = np.full((8, 8), BKGD, np.uint8)
    full = np.full((8, 8), READH, np.uint8)
    full[6:, :] = READ
    for i in range(16):
        cv.pic(64 + 8 * i, 116, empty)
        sd.variant(8 + i, 14, [empty, full])
    # the window is 16 pixels high: the bar cells need cell aligned rows, so draw them at y = 112
    LAYOUT['PSY_COL'] = 8
    LAYOUT['PSY_ROW'] = 14
    return ('ui', sd)


# ------------------------------------------------------- help and end texts
_pages = {}


def pages(kind):
    if kind not in _pages:
        import wl_assets
        d = wl_assets.data()[0]
        _pages[kind] = wl_text.render(wl_text.help_text(d) if kind == 'help' else wl_text.end_text(d))
    return _pages[kind]


def page_screen(kind, i):
    def fn():
        s, _, _ = scrcore.scale_screen(pages(kind)[i])
        # two palettes; the text and the label strip keep their tiles (only the stones of the frame are merged)
        return ('photo', s, dict(limit=448, protect_cells=[(1, 1, 30, 21, 300.0), (0, 22, 31, 23, 1e7)]))
    return fn


def count_pages(kind):
    import wl_assets
    d = wl_assets.data()[0]
    t = wl_text.help_text(d) if kind == 'help' else wl_text.end_text(d)
    return len(wl_text.re.findall(r'\^[Pp]', t))


N_HELP, N_END = count_pages('help'), count_pages('end')
HELP_FIRST = len(SCREENS)
for _i in range(N_HELP):
    SCREENS.append(('help%02d' % _i, page_screen('help', _i)))
END_FIRST = len(SCREENS)
for _i in range(N_END):
    SCREENS.append(('end%02d' % _i, page_screen('end', _i)))


# ------------------------------------------------------------------- build
def screen_names():
    return [n for n, _ in SCREENS]


def make(name):
    """-> (blob, preview rgb)"""
    pal = pal8()
    fn = dict(SCREENS)[name]
    r = fn()
    if r[0] == 'photo':
        return wl_scr.build_photo(name, r[1], pal, **r[2])
    return wl_scr.build_ui(r[1], pal)


def PAGE_DEFS(n):
    return {'SCR_HELP_FIRST': HELP_FIRST, 'HELP_PAGES': N_HELP, 'SCR_END_FIRST': END_FIRST, 'END_PAGES': N_END}


def build(pk, H, C, d, cached):
    banks, addrs = [], []
    for n, fn in SCREENS:
        if not n.startswith(('help', 'end')):
            fn()                # fills LAYOUT (the quantised blobs below come from the cache)
    for i, (n, fn) in enumerate(SCREENS):
        blob = cached('scr_' + n, lambda n=n: make(n)[0])
        b, a = pk.add('scr_' + n, blob)
        banks.append(b)
        addrs.append(a)
        H.append('#define SCR_%s %d' % (n.upper(), i))
    H.append('#define SCR_COUNT %d' % len(SCREENS))
    for k, v in PAGE_DEFS(len(SCREENS)).items():
        H.append('#define %s %d' % (k, v))
    for k, v in LAYOUT.items():
        H.append('#define LAY_%s %d' % (k, v))
    gen = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gen')
    with open(os.path.join(gen, 'scr_tab.c'), 'w') as f:
        f.write('/* generated by tools/wl_pics.py: where each screen is in the ROM */\n')
        f.write('const unsigned char scr_bank[%d] = {%s};\n' % (len(banks), ','.join(str(b) for b in banks)))
        f.write('const unsigned short scr_addr[%d] = {%s};\n' % (len(addrs), ','.join('0x%04X' % a for a in addrs)))
    print('screens: %d, ROM %d bytes' % (len(banks), sum(len(cached('scr_' + n, lambda n=n: make(n)[0])) for n, _ in SCREENS)))


if __name__ == '__main__':
    from PIL import Image
    os.makedirs('/tmp/s', exist_ok=True)
    for n in (sys.argv[1:] or screen_names()):
        blob, prev = make(n)
        ntiles = int.from_bytes(blob[0:2], 'little')
        Image.fromarray(prev).resize((prev.shape[1] * 3, prev.shape[0] * 3), Image.NEAREST).save('/tmp/s/scr_%s.png' % n)
        print('%-12s %3d tiles, blob %5d bytes' % (n, ntiles, len(blob)))
