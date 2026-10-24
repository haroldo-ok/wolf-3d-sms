#include "def.h"
/* wl_game.c - level setup, play screen and the game loop (port of WL_GAME.C) */
#include <string.h>

gametype gamestate;
boolean ingame;

static const s16 ElevatorBackTo[6] = { 1, 1, 7, 3, 5, 3 };

/* ------------------------------------------------------- positioned sound */

/* one PSG channel and no stereo: sounds farther than 20 tiles are skipped */
void PlaySoundLocGlobal (u16 s, s32 gx, s32 gy) B
{
    s32 dx = gx - viewx, dy = gy - viewy;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx > (20L << TILESHIFT) || dy > (20L << TILESHIFT))
        return;
    SD_PlaySound(s);
}

void UpdateSoundLoc (void) B
{
}

/* -------------------------------------------------------- level scanning */

typedef struct { u8 base; u8 en; u8 patrol; } enemygroup_t;
/* enemies of episode 1; +36 medium, +72 hard */
static const enemygroup_t groups[] =
{
    { 108, en_guard, 0 }, { 112, en_guard, 1 },
    { 126, en_ss, 0 }, { 130, en_ss, 1 },
    { 134, en_dog, 0 }, { 138, en_dog, 1 }
};

static void ScanEnemy(u16 tile, s16 x, s16 y)
{
    u8 i;
    for (i = 0; i < sizeof(groups) / sizeof(groups[0]); i++)
    {
        const enemygroup_t *g = &groups[i];
        s16 off = tile - g->base;
        s16 level = 0;
        if (off >= 72) { off -= 72; level = gd_hard; }
        else if (off >= 36) { off -= 36; level = gd_medium; }
        if (off >= 0 && off < 4)
        {
            if (gamestate.difficulty < level)
                return;
            if (g->patrol)
                SpawnPatrol((enemy_t)g->en, x, y, off);
            else
                SpawnStand((enemy_t)g->en, x, y, off);
            return;
        }
    }
}

void ScanInfoPlane (void) B
{
    u16 i, n = LevelObjCount(), key;
    u8 tile;
    s16 x, y;

    for (i = 0; i < n; i++)
    {
        key = LevelObj(i, &tile);
        x = key & 63;
        y = key >> 6;
        if (tile >= 19 && tile <= 22)
            SpawnPlayer(x, y, NORTH + tile - 19);
        else if (tile >= 23 && tile <= 70)
            SpawnStatic(x, y, tile - 23);
        else if (tile == 98)
        {
            if (!loadedgame)
                gamestate.secrettotal++;
        }
        else if (tile == 124)
            SpawnDeadGuard(x, y);
        else if (tile == 214)
            SpawnBoss(x, y);
        else
            ScanEnemy(tile, x, y);
    }
}

void SetupGameLevel (void) B
{
    u8 i, n, d[6];
    u16 k;
    u8 *a, *t;

    if (!loadedgame)
    {
        gamestate.TimeCount =
        gamestate.secrettotal =
        gamestate.killtotal =
        gamestate.treasuretotal =
        gamestate.secretcount =
        gamestate.killcount =
        gamestate.treasurecount = 0;
    }

    US_InitRndT(TRUE);

    slot2_map(SLOT2_CARTRAM);
    LevelLoad(gamestate.mapon);
    vid_set_ceiling(map_ceil[gamestate.mapon]);

    /* what blocks a tile: the walls (the doors and the statics come later) */
    a = (u8 *)actorat;
    t = (u8 *)tilemap;
    for (k = 0; k < 4096; k++)
        a[k] = (t[k] & 0x80) ? 0 : (t[k] & 0x3F);

    InitActorList();
    InitDoorList();
    InitStaticList();
    pwallstate = 0;
    pwallpos = 0;
    cr_pwpos = 0;
    render_cast_dirty = 1;

    n = LevelDoorCount();
    for (i = 0; i < n; i++)
    {
        LevelDoor(i, d);
        SpawnDoor(d[0], d[1], d[2], d[3], d[4], d[5]);
    }

    ScanInfoPlane();
}

/* ------------------------------------------------------------ play screen */

void DrawStatusBar (void) B
{
    DrawFace();
    DrawHealth();
    DrawLives();
    DrawLevel();
    DrawAmmo();
    DrawKeys();
    DrawWeapon();
    DrawScore();
}

void DrawPlayScreen (void) B
{
    vid_fade_out();
    vid_init_game();
    DrawStatusBar();
}

/* ------------------------------------------------------------- death cam */

void DeathCamIntro (void) B
{
    FinishPaletteShifts();
    vid_fade_out();
    ThreeDRefresh();
    vid_fade_in();
}

/* ------------------------------------------------------------------ died */

#define DEATHROTATE 2

void Died (void) B
{
    s32 dx, dy;
    s16 iangle, curangle, clockwise, counter, change;

    gamestate.weapon = (weapontype)-1;
    weapon_hide();
    SD_PlaySound(PLAYERDEATHSND);

    dx = killerobj->x - player->x;
    dy = player->y - killerobj->y;
    iangle = AimAngle(dy, dx);

    if (player->angle > iangle)
    {
        counter = player->angle - iangle;
        clockwise = ANGLES - player->angle + iangle;
    }
    else
    {
        clockwise = iangle - player->angle;
        counter = player->angle + ANGLES - iangle;
    }

    curangle = player->angle;
    if (clockwise < counter)
    {
        if (curangle > iangle) curangle -= ANGLES;
        do
        {
            change = tics * DEATHROTATE;
            if (curangle + change > iangle) change = iangle - curangle;
            curangle += change;
            player->angle += change;
            if (player->angle >= ANGLES) player->angle -= ANGLES;
            ThreeDRefresh();
            CalcTics();
        } while (curangle != iangle);
    }
    else
    {
        if (curangle < iangle) curangle += ANGLES;
        do
        {
            change = -tics * DEATHROTATE;
            if (curangle + change < iangle) change = iangle - curangle;
            curangle += change;
            player->angle += change;
            if (player->angle < 0) player->angle += ANGLES;
            ThreeDRefresh();
            CalcTics();
        } while (curangle != iangle);
    }

    FinishPaletteShifts();

    /* red out the view, then wait for a button */
    IN_ClearKeys();
    Inter_FadeToColor(1);
    IN_UserInput(100);
    SD_WaitSoundDone();

    gamestate.lives--;

    if (gamestate.lives > -1)
    {
        gamestate.health = 100;
        gamestate.weapon = gamestate.bestweapon = gamestate.chosenweapon = wp_pistol;
        gamestate.ammo = STARTAMMO;
        gamestate.keys = 0;
        gamestate.attackframe = gamestate.attackcount = gamestate.weaponframe = 0;

        DrawKeys();
        DrawWeapon();
        DrawAmmo();
        DrawHealth();
        DrawFace();
        DrawLives();
    }
}

/* ------------------------------------------------------------- game loop */

void GameLoop (void) B
{
    boolean died;

    ingame = TRUE;

restartgame:
    StopMusic();
    DrawPlayScreen();
    died = FALSE;
    do
    {
        if (!loadedgame)
            gamestate.score = gamestate.oldscore;
        DrawScore();

        startgame = FALSE;
        if (loadedgame)
            loadedgame = FALSE;
        else
            SetupGameLevel();

        StartMusic();

        if (!died)
            Inter_GetPsyched();
        else
            died = FALSE;

        fizzlein = TRUE;
        DrawLevel();

        /* the first view is drawn while the screen is dark */
        vid_init_game();
        DrawStatusBar();
        ThreeDRefresh();
        vid_fade_in();

        PlayLoop();

        StopMusic();

        if (startgame || loadedgame)
            goto restartgame;

        switch (playstate)
        {
        case ex_completed:
        case ex_secretlevel:
            gamestate.keys = 0;
            DrawKeys();
            vid_fade_out();
            LevelCompleted();
            gamestate.oldscore = gamestate.score;
            if (gamestate.mapon == 9)
                gamestate.mapon = ElevatorBackTo[gamestate.episode];
            else if (playstate == ex_secretlevel)
                gamestate.mapon = 9;
            else
                gamestate.mapon++;
            DrawPlayScreen();
            break;

        case ex_died:
            Died();
            died = TRUE;
            if (gamestate.lives > -1)
                break;
            vid_fade_out();
            ingame = FALSE;
            CheckHighScore(gamestate.score, gamestate.mapon + 1);
            return;

        case ex_victorious:
            vid_fade_out();
            Victory();
            ingame = FALSE;
            CheckHighScore(gamestate.score, gamestate.mapon + 1);
            return;

        default:
            break;
        }
    } while (1);
}
