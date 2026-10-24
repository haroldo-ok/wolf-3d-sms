/* scr.c - full screen pictures and the text, number and cursor cells on them (banked) */
#include "ui.h"

extern const u8 scr_bank[SCR_COUNT];
extern const u16 scr_addr[SCR_COUNT];

u8 scr_cur;
extern u8 vdp_reg1;
static u8 ui_old2;                      /* slot 2 as it was before the front end took it */
u8 ui_active;                           /* a front end screen is on (hud.c does not draw then) */
const ScrHdr *scr_h;
#define H scr_h

static void nt_at(u8 col, u8 row)
{
    vdp_set_addr(VRAM_NAMETABLE + ((u16)row * 32 + col) * 2);
}

static void put(u16 e)
{
    VDPData = e & 0xFF;
    __asm__("nop\n nop\n nop\n nop");
    VDPData = e >> 8;
    __asm__("nop\n nop");
}

void scr_load(u8 id) B
{
    if (!ui_active)
    {
        ui_old2 = slot2_cur;
        ui_active = 1;
    }
    scr_cur = id;
    slot2_map(scr_bank[id]);
    H = (const ScrHdr *)scr_addr[id];
    vdp_display(0);
    vram_write(0, (const u8 *)H + H->tiles_off, H->ntiles * 32);
    vram_write(VRAM_NAMETABLE, (const u8 *)H + H->nt_off, 1536);
    if (H->curs_off)
        vram_write(CURS_T0 * 32, (const u8 *)H + H->curs_off, 12 * 32);
    vdp_set_addr(VRAM_SAT);
    VDPData = 0xD0;
    vdp_reg1 &= ~0x03;                  /* 8x8 sprites, not magnified */
    vdp_reg(1, vdp_reg1);
    vdp_reg(6, 0xFF);
    vdp_reg(7, 0xF0);                   /* the border is entry 0 of the sprite palette: the screen's background */
    vid_set_palette(H->pal, H->pal + 16);
    vid_set_palette_black();
    vdp_display(1);
}

void scr_show(u8 id) B
{
    vid_fade_out();
    scr_load(id);
    vid_fade_in();
}

void scr_leave(void) B
{
    vid_fade_out();
    ui_cursor_off();
    if (ui_active)
    {
        ui_active = 0;
        slot2_map(ui_old2);
    }
}

/* a run of cells in the name table: whole cells only, in the vertical blank */
void ui_text(u8 col, u8 row, const char *s, u8 pal) B
{
    u16 t0 = H->font_t0, e;
    u8 c;
    if (t0 == 0xFFFF)
        return;
    wait_vblank();
    nt_at(col, row);
    for (; *s; s++)
    {
        c = (u8)*s;
        if (c < 32 || c > 126)
            c = '?';
        e = t0 + (c - 32);
        if (pal)
            e |= 0x0800;
        put(e);
    }
}

void ui_clear(u8 col, u8 row, u8 w, u8 h) B
{
    u8 x, y;
    const u16 *nt = (const u16 *)((const u8 *)H + H->nt_off);
    wait_vblank();
    for (y = 0; y < h; y++)
    {
        nt_at(col, row + y);
        for (x = 0; x < w; x++)
            put(nt[(row + y) * 32 + col + x]);
    }
}

void ui_pal(u8 col, u8 row, u8 w, u8 h, u8 pal) B
{
    u8 x, y, b[64];
    u16 a;
    wait_vblank();
    for (y = 0; y < h; y++)
    {
        a = VRAM_NAMETABLE + ((u16)(row + y) * 32 + col) * 2;
        vram_read(a, b, (u16)w * 2);                    /* the cells as they are now (they may hold text) */
        vdp_set_addr(a);
        for (x = 0; x < w; x++)
        {
            VDPData = b[2 * x];
            __asm__("nop\n nop\n nop\n nop");
            VDPData = pal ? (b[2 * x + 1] | 0x08) : (b[2 * x + 1] & ~0x08);
            __asm__("nop\n nop");
        }
    }
}

/* digits 0-9 are glyphs 0-9 of the big font, ':' 10, '%' 11, blank 12: 2 x 2 cells each, ':' only its left column */
static u16 dig(u8 g, u8 part)
{
    return H->dig_t0 + 4 * g + part;
}

void ui_number(u8 col, u8 row, u8 w, s32 v, u8 blank) B
{
    u8 n, i, d[10], nd = 0, ndig = w >> 1;
    u8 g[8];
    if (v < 0)
        v = 0;
    do
    {
        d[nd++] = (u8)(v % 10);
        v /= 10;
    } while (v && nd < 10);
    for (i = 0; i < ndig; i++)
        g[i] = (ndig - 1 - i < nd) ? d[ndig - 1 - i] : 12;
    (void)blank;
    wait_vblank();
    for (n = 0; n < 2; n++)
    {
        nt_at(col, row + n);
        for (i = 0; i < ndig; i++)
        {
            put(dig(g[i], 2 * n));
            put(dig(g[i], 2 * n + 1));
        }
    }
}

void ui_time(u8 col, u8 row, u16 min, u8 sec) B
{
    u8 n;
    u8 g[5];
    g[0] = (u8)(min / 10 % 10);
    g[1] = (u8)(min % 10);
    g[2] = 10;
    g[3] = sec / 10;
    g[4] = sec % 10;
    wait_vblank();
    for (n = 0; n < 2; n++)
    {
        u8 i;
        nt_at(col, row + n);
        for (i = 0; i < 5; i++)
        {
            put(dig(g[i], 2 * n));
            if (i != 2)
                put(dig(g[i], 2 * n + 1));
        }
    }
}

/* variant v of rectangle rec */
void ui_variant(u8 rec, u8 v) B
{
    const ScrVar *r = (const ScrVar *)((const u8 *)H + H->vars_off) + rec;
    const u16 *d = (const u16 *)((const u8 *)H + r->off) + (u16)v * r->w * r->h;
    u8 x, y;
    wait_vblank();
    for (y = 0; y < r->h; y++)
    {
        nt_at(r->col, r->row + y);
        for (x = 0; x < r->w; x++)
            put(d[y * r->w + x]);
    }
}

/* the cursor: 3 x 2 sprites of 8 x 8, tiles in column order */
void ui_cursor(u8 x, u8 y, u8 frame) B
{
    u8 cx, cy, t;
    wait_vblank();
    vdp_set_addr(VRAM_SAT);
    for (cx = 0; cx < 3; cx++)
        for (cy = 0; cy < 2; cy++)
        {
            VDPData = y + 8 * cy - 1;
            __asm__("nop\n nop\n nop\n nop");
        }
    VDPData = 0xD0;
    vdp_set_addr(VRAM_SAT + 0x80);
    for (cx = 0; cx < 3; cx++)
        for (cy = 0; cy < 2; cy++)
        {
            t = (CURS_T0 - 256) + frame * 6 + cx * 2 + cy;
            VDPData = x + 8 * cx;
            __asm__("nop\n nop\n nop\n nop");
            VDPData = t;
            __asm__("nop\n nop\n nop\n nop");
        }
}

void ui_cursor_off(void) B
{
    wait_vblank();
    vdp_set_addr(VRAM_SAT);
    VDPData = 0xD0;
}
