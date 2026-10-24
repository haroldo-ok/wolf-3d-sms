"""The help and end texts of Wolfenstein 3-D (T_HELPART, T_ENDART1 of VGAGRAPH): the original page layout engine
(WL_TEXT.C: margins, ^-commands, pictures with the text flowing around them) on a 320 x 200 canvas of VGA palette
indices, one canvas per page.  The pages that describe the keyboard are rewritten for the Master System's pad."""
import re
import numpy as np
import wl_gfx, wl_scr

BACKCOLOR = 0x11
TOPMARGIN, LEFTMARGIN, RIGHTMARGIN, FONTHEIGHT = 16, 16, 16, 10
TEXTROWS = (200 - TOPMARGIN - 32) // FONTHEIGHT
PICMARGIN, SPACEWIDTH, SCREENMID = 8, 7, 160

# pages 1, 6-9 and the last: the original describes keyboard, mouse and joystick
SMS_PAGES = {
    1: """^PAGE 1
^c7eQUICK COMMAND REFERENCE
^c00
The D-pad moves and turns. Button 1 shoots.

Button 2:\t\tuse (doors, push walls)
Button 2 + Left/Right:\tstrafe
Button 2 + Down:\t\tnext weapon
PAUSE (console):\t\tmenu
In menus: Up/Down moves,
Button 1 selects, Button 2 backs out

Read on for more information.
""",
    6: """^PAGE 6
^G16,16,6
^c7eCONTROLS
^c00
In Wolfenstein 3-D for the Master System, use the control pad.

To move around the menus, press ^c6eUp ^c00and ^c6eDown ^c00to move from item to item, then press ^c6eButton 1 ^c00to select that item.  ^c6eButton 2 ^c00always backs out.

The console's ^c6ePAUSE ^c00button opens the menu at any time during the game.
""",
    7: """^PAGE 7
Once you are in the game, use the D-pad to move: up and down walk, left and right turn.

Press ^c6eButton 1 ^c00to fire your weapon.

Hold ^c6eButton 2 ^c00and press left or right to slide side to side instead of turning left or right.  This is called "Strafe Mode."

Press ^c6eButton 2 ^c00to Open Doors and activate elevators to go to the next floor of the keep.
""",
    8: """^PAGE 8
^G16,16,6
Hold ^c6eButton 2 ^c00and press ^c6eDown ^c00to step through the weapons you are carrying.



Your best weapon is selected automatically when you pick one up.
""",
    9: """^PAGE 9
^G50,110,12
Once you have more than one weapon, ^c6eButton 2 + Down ^c00chooses the next one:







Holding down the fire button is fun, but short bursts save bullets.
""",
}


def help_text(d):
    t = d.vga_chunk(150).decode('latin1').replace('\r', '')
    parts = re.split(r'(?=^\^PAGE ?\d+\n)', t, flags=re.M)
    out = []
    for p in parts:
        m = re.match(r'\^PAGE ?(\d+)\n', p)
        if m and int(m.group(1)) in SMS_PAGES:
            out.append(SMS_PAGES[int(m.group(1))])
        else:
            out.append(p)
    t = ''.join(out)
    t = t.replace("Hold down Space to push the wall in front of you. Just think of Space as the Use key.",
                  "Press Button 2 to push the wall in front of you. Just think of Button 2 as the Use key.")
    t = t.replace("(ESC to return to menu.)", "(Button 2 to return to menu.)")
    return t


def end_text(d):
    return d.vga_chunk(155).decode('latin1').replace('\r', '')


class Layout:
    def __init__(self, text):
        self.text = text
        self.pos = 0
        self.pages = []

    # ------------------------------------------------------------- parsing helpers
    def peek(self):
        return self.text[self.pos] if self.pos < len(self.text) else '\0'

    def number(self):
        while self.pos < len(self.text) and not self.text[self.pos].isdigit():
            self.pos += 1
        n = 0
        while self.pos < len(self.text) and self.text[self.pos].isdigit():
            n = n * 10 + int(self.text[self.pos])
            self.pos += 1
        return n

    def rip_eol(self):
        while self.pos < len(self.text) and self.text[self.pos] != '\n':
            self.pos += 1
        self.pos += 1

    def count_pages(self):
        return len(re.findall(r'\^[Pp]', self.text))

    # ------------------------------------------------------------------- one page
    def page(self, num, total):
        cv = wl_scr.Canvas(BACKCOLOR, h=200, w=320)
        cv.pic(0, 0, 'H_TOPWINDOWPIC')
        cv.pic(0, 8, 'H_LEFTWINDOWPIC')
        cv.pic(312, 8, 'H_RIGHTWINDOWPIC')
        cv.pic(8, 176, 'H_BOTTOMINFOPIC')
        # the button label of the original says ESC: the Master System's button 2 backs out
        strip = wl_gfx.pic256('H_BOTTOMINFOPIC')
        box = int(strip[4, 70])
        region = strip[6:18, 66:110]
        lum = wl_scr.pal8()[region].astype(int).sum(2)
        ink = int(region.reshape(-1)[int(np.argmin(lum))])
        cv.bar(8 + 66, 176 + 4, 44, 14, box)
        cv.text(0, 8 + 69, 176 + 7, '2=EXIT', ink)
        self.cv = cv
        self.left = [LEFTMARGIN] * TEXTROWS
        self.right = [320 - RIGHTMARGIN] * TEXTROWS
        self.px, self.py, self.row = LEFTMARGIN, TOPMARGIN, 0
        self.done = False
        self.color = 0
        t = self.text
        while self.pos < len(t) and ord(t[self.pos]) <= 32:
            self.pos += 1
        self.pos += 2                           # ^P
        self.rip_eol()
        while not self.done and self.pos < len(t):
            ch = t[self.pos]
            if ch == '^':
                self.command()
            elif ch == '\t':
                self.px = (self.px + 8) & ~7
                self.pos += 1
            elif ord(ch) <= 32:
                self.pos += 1
                if ch == '\n':
                    self.newline()
            else:
                self.word()
        cv.text(0, 213, 183, 'pg %d of %d' % (num, total), 0x4F)
        return cv.p.copy()

    def newline(self):
        self.row += 1
        if self.row == TEXTROWS:
            self.done = True
            t = self.text
            while self.pos < len(t):                 # the rest of the page that does not fit is dropped
                if t[self.pos] == '^' and t[self.pos + 1:self.pos + 2] in ('E', 'e', 'P', 'p'):
                    return
                self.pos += 1
            return
        self.px = self.left[self.row]
        self.py += FONTHEIGHT

    def command(self):
        t = self.text
        self.pos += 1
        c = t[self.pos].upper()
        if c == 'B':
            self.pos += 1
            y, x, w, h = self.number(), self.number(), self.number(), self.number()
            self.cv.bar(x, y, w, h, BACKCOLOR)
            self.rip_eol()
        elif c == ';':
            self.rip_eol()
        elif c in 'PE':
            self.done = True
            self.pos -= 1
        elif c == 'C':
            v = int(t[self.pos + 1:self.pos + 3], 16)
            self.color = v
            self.pos += 3
        elif c == '>':
            self.px = 160
            self.pos += 1
        elif c == 'L':
            self.pos += 1
            y = self.number()
            self.row = (y - TOPMARGIN) // FONTHEIGHT
            self.py = TOPMARGIN + self.row * FONTHEIGHT
            self.px = self.number()
            self.rip_eol()
        elif c in 'TG':
            self.pos += 1
            y, x, pic = self.number(), self.number(), self.number()
            if c == 'T':
                self.number()
            self.rip_eol()
            name = [n for n, v in wl_gfx_names().items() if v == pic][0]
            a = wl_gfx.pic256(name)
            self.cv.pic(x & ~7, y, a)
            h, w = a.shape
            mid = x + w // 2
            margin = x - PICMARGIN if mid > SCREENMID else x + w + PICMARGIN
            top = max(0, (y - TOPMARGIN) // FONTHEIGHT)
            bottom = min(TEXTROWS - 1, (y + h - TOPMARGIN) // FONTHEIGHT)
            for i in range(top, bottom + 1):
                if mid > SCREENMID:
                    self.right[i] = margin
                else:
                    self.left[i] = margin
            if self.px < self.left[self.row]:
                self.px = self.left[self.row]
        else:
            self.pos += 1

    def word(self):
        t = self.text
        a = self.pos
        self.pos += 1
        while self.pos < len(t) and ord(t[self.pos]) > 32 and self.pos - a < 79:
            self.pos += 1
        w = t[a:self.pos]
        width = wl_scr.text_width(0, w)
        while self.px + width > self.right[self.row]:
            self.newline()
            if self.done:
                return
        self.cv.text(0, self.px, self.py, w, self.color)
        self.px += width
        while self.pos < len(t) and t[self.pos] == ' ':
            self.px += SPACEWIDTH
            self.pos += 1


_names = {}


def wl_gfx_names():
    if not _names:
        import wl_names
        _names.update(dict(wl_names.gfx_names()))
    return _names


def render(text):
    """all pages of a text -> list of 200 x 320 index arrays"""
    lay = Layout(text)
    total = lay.count_pages()
    pages = []
    for i in range(total):
        pages.append(lay.page(i + 1, total))
    return pages


def build(pk, H, C, d, cached):
    """nothing to add to the ROM here: the pages are screens, wl_pics.py packs them"""
    pass
