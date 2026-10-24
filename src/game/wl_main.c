#include "def.h"
/* wl_main.c - random numbers and new game (port of WL_MAIN.C) */
#include <string.h>

static u8 rndindex;

void US_InitRndT (boolean randomize) B
{
#ifdef DEBUG_DET
    (void)randomize;
    rndindex = 0;                   /* deterministic build: tests/det */
#else
    if (randomize)
        rndindex = (u8)(get_timecount() + frame_count);
    else
        rndindex = 0;
#endif
}

u16 US_RndT (void) B
{
    rndindex++;
    return rndtable[rndindex];
}

void NewGame (s16 difficulty, s16 episode) B
{
    memset(&gamestate, 0, sizeof(gamestate));
    gamestate.difficulty = difficulty;
    gamestate.weapon = gamestate.bestweapon = gamestate.chosenweapon = wp_pistol;
    gamestate.health = 100;
    gamestate.ammo = STARTAMMO;
    gamestate.lives = 3;
    gamestate.nextextra = EXTRAPOINTS;
    gamestate.episode = episode;
    startgame = FALSE;
    loadedgame = FALSE;
}
