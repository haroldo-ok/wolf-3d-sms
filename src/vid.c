/* vid.c - screen modes, palette fades and flashes, the fizzle fade (banked) */
#include "game/def.h"

static u8 pal_base[32];         /* the palettes of the screen (CRAM image) */
static u8 pal_ceiling;
u8 screenfaded;

/* the palette flashes (damage red, pickup yellow): the CRAM image last written by tint() / fade_level(); a flash
 * that is already on screen is not written again (0xFF: unknown) */
static u8 flash_cur = 0xFF;

/* tint every entry: each channel flagged in `rgb` rises by `level`, the others fall by level / 3 of their
 * value (rounded up: v - (v * level + 2) / 3, tables instead of the divisions); level 0 = unchanged */
static const u8 fall_tab[4][4] = { { 0, 1, 2, 3 }, { 0, 0, 1, 2 }, { 0, 0, 0, 1 }, { 0, 0, 0, 0 } };
static const u8 rise_tab[4][4] = { { 0, 1, 2, 3 }, { 1, 2, 3, 3 }, { 2, 3, 3, 3 }, { 3, 3, 3, 3 } };

static void tint(u8 rflag, u8 gflag, u8 bflag, u8 level)
{
    u8 i, buf[32];
    const u8 *tr = rflag ? rise_tab[level] : fall_tab[level];
    const u8 *tg = gflag ? rise_tab[level] : fall_tab[level];
    const u8 *tb = bflag ? rise_tab[level] : fall_tab[level];
    for (i = 0; i < 32; i++)
    {
        u8 c = pal_base[i];
        buf[i] = tr[c & 3] | (tg[(c >> 2) & 3] << 2) | (tb[c >> 4] << 4);
    }
    cram_load(buf, 0, 32);
}

static void fade_level(u8 k)            /* 0 normal .. 3 black */
{
    u8 i, buf[32];
    flash_cur = 0xFF;
    for (i = 0; i < 32; i++)
    {
        u8 c = pal_base[i];
        u8 r = c & 3, g = (c >> 2) & 3, b = (c >> 4) & 3;
        r = r > k ? r - k : 0;
        g = g > k ? g - k : 0;
        b = b > k ? b - k : 0;
        buf[i] = r | (g << 2) | (b << 4);
    }
    cram_load(buf, 0, 32);
}

void vid_set_palette(const u8 *p0, const u8 *p1) B
{
    u8 i;
    flash_cur = 0xFF;
    for (i = 0; i < 16; i++)
    {
        pal_base[i] = p0[i];
        pal_base[16 + i] = p1[i];
    }
}

void vid_set_palette_black(void) B
{
    fade_level(3);
    screenfaded = 1;
}

void vid_set_palette_normal(void) B
{
    fade_level(0);
    screenfaded = 0;
}

void vid_wait(u16 frames) B
{
    while (frames--)
        wait_vblank();
}

void vid_fade_out(void) B
{
    u8 k;
    if (screenfaded)
        return;
    for (k = 1; k <= 3; k++)
    {
        vid_wait(2);
        fade_level(k);
    }
    screenfaded = 1;
}

void vid_fade_in(void) B
{
    s8 k;
    for (k = 2; k >= 0; k--)
    {
        vid_wait(2);
        fade_level(k);
    }
    screenfaded = 0;
}

void vid_flash(u8 r, u8 g, u8 b, u8 level) B
{
    u8 key = level ? (u8)(r | (g << 1) | (b << 2) | (level << 3)) : 0;
    if (key == flash_cur)
        return;
    if (!level)
        fade_level(0);
    else
        tint(r, g, b, level);
    flash_cur = key;
}

/* ceiling colour of a level: palette entry 1 (the floor is entry 0) */
void vid_set_ceiling(u8 map_ceil) B
{
    flash_cur = 0xFF;
    pal_base[1] = (map_ceil == 0xbf) ? CEIL_PAL_E1 : view_pal[1];
}

/* overscan colour (VDP register 7 = sprite palette entry) */
void VW_ColorBorder(u8 color) B
{
    vdp_reg(7, 0xF0 | (color & 15));
}

void nt_fill(u16 tile) B
{
    u16 i;
    vdp_set_addr(VRAM_NAMETABLE);
    for (i = 0; i < 768; i++)
    {
        VDPData = tile & 0xFF;
        __asm__("nop\n nop");
        VDPData = tile >> 8;
    }
}

/* the play screen: the view, the HUD and the weapon; display on, palettes
 * as they were (black while screenfaded) */
void vid_init_game(void) B
{
    vdp_display(0);
    vid_set_palette(view_pal, hud_pal);
    vram_fill(0, 0, 0x3800);
    nt_fill(TILE_BLANK | 0x0800);
    render_init();
    hud_load();
    weapon_load();
    render_invalidate();
    VW_ColorBorder(0);
    if (screenfaded)
        fade_level(3);
    else
        fade_level(0);
    vdp_display(1);
}

/* ---------------------------------------------------------------- fizzle
 * The new view appears cell by cell in pseudo random order (the original's
 * pixel LFSR at tile resolution).  fizzle_begin() blanks the view and stops
 * the renderer from writing the nametable; after render_draw,
 * FizzleFade() reveals the cells nt_state says are right. */
static void view_cell(u8 x, u8 y, u16 tile)
{
    vram_put_word(VRAM_NAMETABLE + ((((u16)(VIEW_TY0 + y)) << 5) + VIEW_TX0 + x) * 2, tile);
}

void fizzle_begin(void) B
{
    u8 x, y;
    for (y = 0; y < VIEW_TH; y++)
        for (x = 0; x < VIEW_TW; x++)
            view_cell(x, y, TILE_BLANK);
    nt_hold = 1;
}

void FizzleFade(void) B
{
    u16 lfsr = 1, cell;
    u8 n = 0, x, y, st;
    nt_hold = 0;
    /* 9 bit maximal LFSR (x^9 + x^5 + 1): 511 states cover the 280 cells */
    do
    {
        u16 bit = (lfsr ^ (lfsr >> 4)) & 1;
        lfsr = (lfsr >> 1) | (bit << 8);
        cell = lfsr - 1;
        if (cell < VIEW_TW * VIEW_TH)
        {
            x = cell / VIEW_TH;
            y = cell % VIEW_TH;
            st = nt_state[cell];
            view_cell(x, y, st == 0 ? TILE_SKY : st == 1 ? TILE_GROUND : cell);
            if (++n == 12)
            {
                n = 0;
                wait_vblank();
            }
        }
    } while (lfsr != 1);
}
