/* hud.c - the status area below the view (banked).  Tiles and layout come
 * from tools/wl_hud.py; the numbers are digit glyph tiles in the nametable,
 * the face, weapon icon and keys are uploaded / switched when they change. */
#include "game/def.h"

#define NT_ADDR(c, r)   (VRAM_NAMETABLE + ((((u16)(HUD_ROW0 + (r))) << 5) + (c)) * 2)
#define HUD_PAL1        0x0800          /* nametable bit: palette 1 */

static u8 hud_face_cur, hud_icon_cur, hud_keys_cur;

extern u8 ui_active;

void hud_load(void) B
{
    u8 row, buf[64], i;
    rom_to_vram(HUD_STATIC_BANK, (const void *)HUD_STATIC_ADDR, HUD_T0 * 32, HUD_STATIC_TILES * 32);
    for (row = 0; row < 12; row++)
    {
        rom_copy(HUD_NT_BANK, (const void *)(HUD_NT_ADDR + row * 64), buf, 64);
        for (i = 0; i < 32; i++)
        {
            u16 w = (buf[2 * i] | (buf[2 * i + 1] << 8)) & 0x7FF;
            w += HUD_T0;
            buf[2 * i] = w & 0xFF;
            buf[2 * i + 1] = (w >> 8) | 0x08;
        }
        vdp_set_addr(NT_ADDR(0, row));
        for (i = 0; i < 64; i++)
        {
            VDPData = buf[i];
            __asm__("nop\n nop\n nop");
        }
    }
    hud_face_cur = hud_icon_cur = hud_keys_cur = 0xFF;
}

/* right justified, blank padded; value < 0 counts as 0 */
static const u8 hud_num_c[5] = { HUD_FLOOR_C, HUD_SCORE_C, HUD_LIVES_C, HUD_HEALTH_C, HUD_AMMO_C };
static const u8 hud_num_r[5] = { HUD_FLOOR_R, HUD_SCORE_R, HUD_LIVES_R, HUD_HEALTH_R, HUD_AMMO_R };
static const u8 hud_num_n[5] = { HUD_FLOOR_N, HUD_SCORE_N, HUD_LIVES_N, HUD_HEALTH_N, HUD_AMMO_N };

void hud_number(u8 which, s32 value) B
{
    if (ui_active)
        return;                         /* the front end owns the screen: the play screen is redrawn when it is left */
    u8 digits[6], n = hud_num_n[which], i, c = hud_num_c[which], r = hud_num_r[which];
    u8 len = 0;
    if (value < 0)
        value = 0;
    do
    {
        digits[len++] = (u8)(value % 10);
        value /= 10;
    } while (value && len < n);
    for (i = 0; i < n; i++)
    {
        u16 t = HUD_T0 + HUD_T_DIG + 2 * (i < len ? digits[n - 1 - i < len ? n - 1 - i : 0] : 10);
        /* digit i of n from the left: position from the right = n - 1 - i */
        u8 pos = n - 1 - i;
        if (pos < len)
            t = HUD_T0 + HUD_T_DIG + 2 * digits[pos];
        else
            t = HUD_T0 + HUD_T_DIG + 20;                /* blank */
        vram_put_word(NT_ADDR(c + i, r), t | HUD_PAL1);
        vram_put_word(NT_ADDR(c + i, r + 1), (t + 1) | HUD_PAL1);
    }
    if (which == HUD_HEALTH)
    {
        /* the percent sign sits after the digits (static) */
    }
}

void hud_face(u8 pic) B
{
    if (ui_active)
        return;                         /* the front end owns the screen: the play screen is redrawn when it is left */
    u8 bank;
    u16 addr;
    if (pic == hud_face_cur)
        return;
    hud_face_cur = pic;
    bank = HUD_FACES_BANK0 + pic / HUD_FACES_PER_BANK;
    addr = 0x8000 + (pic % HUD_FACES_PER_BANK) * (HUD_FACE_W * HUD_FACE_H * 32);
    rom_to_vram_safe(bank, (const void *)addr, (HUD_T0 + HUD_T_FACE) * 32, HUD_FACE_W * HUD_FACE_H * 32);
}

void hud_weapon(u8 weapon) B
{
    if (ui_active)
        return;                         /* the front end owns the screen: the play screen is redrawn when it is left */
    if (weapon == hud_icon_cur || weapon > 3)
        return;
    hud_icon_cur = weapon;
    rom_to_vram_safe(HUD_ICONS_BANK, (const void *)(HUD_ICONS_ADDR + weapon * (HUD_ICON_W * HUD_ICON_H * 32)),
                     (HUD_T0 + HUD_T_ICON) * 32, HUD_ICON_W * HUD_ICON_H * 32);
}

void hud_keys(u8 keys) B
{
    if (ui_active)
        return;                         /* the front end owns the screen: the play screen is redrawn when it is left */
    u16 t;
    if (keys == hud_keys_cur)
        return;
    hud_keys_cur = keys;
    t = HUD_T0 + HUD_T_KEYS + ((keys & 1) ? 2 : 0);
    vram_put_word(NT_ADDR(HUD_KEYGOLD_C, HUD_KEYGOLD_R), t | HUD_PAL1);
    vram_put_word(NT_ADDR(HUD_KEYGOLD_C, HUD_KEYGOLD_R + 1), (t + 1) | HUD_PAL1);
    t = HUD_T0 + HUD_T_KEYS + ((keys & 2) ? 4 : 0);
    vram_put_word(NT_ADDR(HUD_KEYSILVER_C, HUD_KEYSILVER_R), t | HUD_PAL1);
    vram_put_word(NT_ADDR(HUD_KEYSILVER_C, HUD_KEYSILVER_R + 1), (t + 1) | HUD_PAL1);
}
