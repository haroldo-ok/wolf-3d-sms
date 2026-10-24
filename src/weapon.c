/* weapon.c - the player's weapon: up to 40 hardware sprites (8x8, magnified
 * 2x) over the view, 8 columns x 5 rows.  The tiles of the current frame are
 * uploaded when the frame changes (tools/wl_hud.py, weapon_update in render.c). */
#include "game/def.h"

extern u8 vdp_reg1;
#define NOFRAME     0xFF

extern u8 wp_frame;               /* frame the sprites show (render.c) */

void weapon_load(void) B
{
    /* 8x8 sprites magnified 2x, sprite tiles from 0x2000 */
    vdp_reg1 = (vdp_reg1 & ~0x02) | 0x01;
    vdp_reg(1, vdp_reg1);
    vdp_reg(6, 0xFF);
    wp_frame = NOFRAME;
    vdp_set_addr(VRAM_SAT);
    VDPData = 0xD0;
}

void weapon_hide(void) B
{
    wp_frame = NOFRAME;
    vdp_set_addr(VRAM_SAT);
    VDPData = 0xD0;
}

void weapon_show(u8 weapon, u8 frame) B
{
    (void)weapon; (void)frame;
}

