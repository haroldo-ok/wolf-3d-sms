/* main.c - startup and the top level loop (bank 0) */
#include "game/def.h"
#include "ui.h"

void vid_set_palette(const u8 *p0, const u8 *p1) B;
void Scores_Defaults(void) B;
void snd_init(void) B;
void NewGame(s16 difficulty, s16 episode) B;
void GameLoop(void) B;

void main(void)
{
    hw_init();
    vdp_display(0);
    vram_fill(0, 0, 0x4000);
    vid_set_palette(view_pal, hud_pal);
    vid_set_palette_normal();
    SD_Startup();
    snd_init();
    Scores_Defaults();
    Scores_Load();
#ifdef DEBUG_MIRRORS
    for (;;)                            /* the tests start in the game */
    {
        NewGame(gd_medium, 0);
        startgame = TRUE;
        GameLoop();
    }
#else
    for (;;)
        title_loop();
#endif
}
