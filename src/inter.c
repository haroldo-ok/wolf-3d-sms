/* inter.c - the screens between levels, victory, high scores, the title loop (port of WL_INTER.C) (banked) */
#include "ui.h"
#include "snd.h"
#include <string.h>

void ViewScores(void) B;
void EndText(void) B;
void US_ControlPanel(void) B;
void GameLoop(void) B;
void StartMusic(void) B;

u8 ratio_kill[8], ratio_secret[8], ratio_treasure[8];
u16 ratio_time[8];

/* par times of episode 1 in seconds (the secret level and the boss have none) */
static const u16 par_sec[8] = { 90, 120, 120, 210, 180, 180, 150, 150 };

#ifdef DEBUG_MIRRORS
u8 dbg_noskip;                          /* the test builds run unattended: no screens that wait for a button */
#define DBG_SKIP()  do { if (!dbg_noskip) return; } while (0)
#else
#define DBG_SKIP()  do { } while (0)
#endif

/* ------------------------------------------------------------ helpers */
static void BJ_Breathe(void)
{
    static u8 which = 0;
    static u32 last = 0;
    static u16 max = 10;
    u32 now = get_timecount();
    if (now - last > max)
    {
        which ^= 1;
        ui_variant(0, which);
        last = now;
        max = 35;
    }
}

static void wait_snd(void)
{
    while (SD_SoundPlaying())
    {
        wait_vblank();
        BJ_Breathe();
    }
}

static void put_num(u8 col, u8 row, u8 w, s32 v)
{
    ui_number(col, row, w, v, 0);
}

/* the ratio counts up to `ratio` with the bonus sounds; TRUE: a button ended it */
static u8 ratio_count(s16 ratio, u8 col, u8 row, s32 *bonus)
{
    s16 i;
    for (i = 0; i <= ratio; i++)
    {
        put_num(col, row, LAY_INTER_KILL_W, i);
        if (!(i % 10))
            SD_PlaySound(ENDBONUS1SND);
        wait_snd();
        wait_vblank();
        if (IN_CheckAck())
            return 1;
    }
    if (ratio == 100)
    {
        vid_wait(30);
        SD_StopSound();
        *bonus += 10000;
        put_num(LAY_INTER_BONUS_COL, LAY_INTER_BONUS_ROW, LAY_INTER_BONUS_W, *bonus);
        SD_PlaySound(PERCENT100SND);
    }
    else if (!ratio)
    {
        vid_wait(30);
        SD_StopSound();
        SD_PlaySound(NOBONUSSND);
    }
    else
        SD_PlaySound(ENDBONUS2SND);
    wait_snd();
    return 0;
}

void LevelCompleted(void) B
{
    DBG_SKIP();
    s16 mapon = gamestate.mapon, i, kr = 0, sr = 0, tr = 0, min, sec;
    s32 bonus, timeleft = 0;
    u8 abort = 0;
    scr_load(mapon < 8 ? SCR_INTER : SCR_INTER2);
    SD_StartMusic(ENDLEVEL_MUS);
    IN_ClearKeys();
    if (mapon < 8)
    {
        put_num(LAY_INTER_FLOOR_COL, LAY_INTER_FLOOR_ROW, LAY_INTER_FLOOR_W, mapon + 1);
        ui_time(LAY_INTER_PAR_COL, LAY_INTER_PAR_ROW, par_sec[mapon] / 60, par_sec[mapon] % 60);
        sec = (s16)(gamestate.TimeCount / 70);
        if (sec > 99 * 60)
            sec = 99 * 60;
        if (gamestate.TimeCount < (s32)par_sec[mapon] * 70)
            timeleft = par_sec[mapon] - sec;
        min = sec / 60;
        sec %= 60;
        ui_time(LAY_INTER_TIME_COL, LAY_INTER_TIME_ROW, min, (u8)sec);
        put_num(LAY_INTER_BONUS_COL, LAY_INTER_BONUS_ROW, LAY_INTER_BONUS_W, 0);
        put_num(LAY_INTER_SCORE_COL, LAY_INTER_SCORE_ROW, LAY_INTER_SCORE_W, gamestate.score);
        put_num(LAY_INTER_KILL_COL, LAY_INTER_KILL_ROW, LAY_INTER_KILL_W, 0);
        put_num(LAY_INTER_SECRET_COL, LAY_INTER_SECRET_ROW, LAY_INTER_SECRET_W, 0);
        put_num(LAY_INTER_TREAS_COL, LAY_INTER_TREAS_ROW, LAY_INTER_TREAS_W, 0);
        vid_fade_in();

        if (gamestate.killtotal)
            kr = (s16)((gamestate.killcount * 100L) / gamestate.killtotal);
        if (gamestate.secrettotal)
            sr = (s16)((gamestate.secretcount * 100L) / gamestate.secrettotal);
        if (gamestate.treasuretotal)
            tr = (s16)((gamestate.treasurecount * 100L) / gamestate.treasuretotal);

        bonus = timeleft * 500;
        if (bonus)
        {
            for (i = 0; i <= timeleft; i++)
            {
                put_num(LAY_INTER_BONUS_COL, LAY_INTER_BONUS_ROW, LAY_INTER_BONUS_W, (s32)i * 500);
                if (!(i % 50))
                    SD_PlaySound(ENDBONUS1SND);
                wait_snd();
                wait_vblank();
                if (IN_CheckAck())
                {
                    abort = 1;
                    break;
                }
            }
            if (!abort)
            {
                SD_PlaySound(ENDBONUS2SND);
                wait_snd();
            }
        }
        if (!abort) abort = ratio_count(kr, LAY_INTER_KILL_COL, LAY_INTER_KILL_ROW, &bonus);
        if (!abort) abort = ratio_count(sr, LAY_INTER_SECRET_COL, LAY_INTER_SECRET_ROW, &bonus);
        if (!abort) abort = ratio_count(tr, LAY_INTER_TREAS_COL, LAY_INTER_TREAS_ROW, &bonus);

        /* the final state, also when a button ended the counting */
        put_num(LAY_INTER_KILL_COL, LAY_INTER_KILL_ROW, LAY_INTER_KILL_W, kr);
        put_num(LAY_INTER_SECRET_COL, LAY_INTER_SECRET_ROW, LAY_INTER_SECRET_W, sr);
        put_num(LAY_INTER_TREAS_COL, LAY_INTER_TREAS_ROW, LAY_INTER_TREAS_W, tr);
        bonus = timeleft * 500 + (kr == 100 ? 10000L : 0) + (sr == 100 ? 10000L : 0) + (tr == 100 ? 10000L : 0);
        GivePoints(bonus);
        put_num(LAY_INTER_BONUS_COL, LAY_INTER_BONUS_ROW, LAY_INTER_BONUS_W, bonus);
        ratio_kill[mapon] = (u8)kr;
        ratio_secret[mapon] = (u8)sr;
        ratio_treasure[mapon] = (u8)tr;
        ratio_time[mapon] = min * 60 + sec;
    }
    else
    {
        vid_fade_in();
        GivePoints(15000);
    }
    put_num(LAY_INTER_SCORE_COL, LAY_INTER_SCORE_ROW, LAY_INTER_SCORE_W, gamestate.score);
    IN_ClearKeys();
    set_timecount(0);
    for (;;)
    {
        wait_vblank();
        BJ_Breathe();
        if (IN_CheckAck())
            break;
    }
    scr_leave();
}

void Victory(void) B
{
    DBG_SKIP();
    s16 i, min, kr = 0, sr = 0, tr = 0;
    s32 sec = 0;
    scr_load(SCR_VICTORY);
    SD_StartMusic(URAHERO_MUS);
    for (i = 0; i < 8; i++)
    {
        sec += ratio_time[i];
        kr += ratio_kill[i];
        sr += ratio_secret[i];
        tr += ratio_treasure[i];
    }
    kr /= 8;
    sr /= 8;
    tr /= 8;
    min = (s16)(sec / 60);
    sec %= 60;
    if (min > 99)
        min = sec = 99;
    ui_time(LAY_VICT_TIME_COL, LAY_VICT_TIME_ROW, min, (u8)sec);
    put_num(LAY_VICT_KILL_COL, LAY_VICT_KILL_ROW, LAY_VICT_KILL_W, kr);
    put_num(LAY_VICT_SECRET_COL, LAY_VICT_SECRET_ROW, LAY_VICT_SECRET_W, sr);
    put_num(LAY_VICT_TREAS_COL, LAY_VICT_TREAS_ROW, LAY_VICT_TREAS_W, tr);
    vid_fade_in();
    IN_Ack();
    scr_leave();
    EndText();
}

/* the level is loading: a bar fills */
void Inter_GetPsyched(void) B
{
    DBG_SKIP();
    u8 i;
    scr_load(SCR_PSYCHED);
    vid_fade_in();
    for (i = 0; i < 16; i++)
    {
        ui_variant(i, 1);
        vid_wait(3);
    }
    vid_wait(25);
    scr_leave();
}

/* the view turns red when the player has died */
void Inter_FadeToColor(u8 color) B
{
    DBG_SKIP();
    (void)color;
    vid_flash(1, 0, 0, 3);
}

/* ---------------------------------------------------------- high scores */
#define MAXSCORES   7
#define NAMELEN     15
typedef struct
{
    char name[NAMELEN + 1];
    u16 completed;
    s32 score;
} HighScore;
HighScore Scores[MAXSCORES];

static const char *const defname[MAXSCORES] =
{
    "id software-'92", "Adrian Carmack", "John Carmack", "Kevin Cloud", "Tom Hall", "John Romero", "Jay Wilbur"
};

void Scores_Defaults(void) B
{
    u8 i;
    for (i = 0; i < MAXSCORES; i++)
    {
        memset(Scores[i].name, 0, NAMELEN + 1);
        strcpy(Scores[i].name, defname[i]);
        Scores[i].completed = 1;
        Scores[i].score = 10000;
    }
}

void *Scores_Ptr(void) B { return Scores; }
u16 Scores_Size(void) B { return sizeof(Scores); }

static void draw_scores(void)
{
    u8 i;
    char b[12];
    scr_load(SCR_HSCORE);
    for (i = 0; i < MAXSCORES; i++)
    {
        u8 row = LAY_HS_ROW0 + 2 * i;
        ui_text(LAY_HS_NAME_COL, row, Scores[i].name, 0);
        ui_pad(b, Scores[i].completed, 2);
        ui_text(LAY_HS_LEVEL_COL, row, b, 0);
        ui_pad(b, Scores[i].score, 8);
        ui_text(LAY_HS_SCORE_COL, row, b, 0);
    }
}

static const char name_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-'!";

static void name_entry(char *name)
{
    char buf[NAMELEN + 1];
    u8 pos = 0, i, anim = 0, edge, redraw = 1, n = sizeof(name_chars) - 1;
    memset(buf, ' ', NAMELEN);
    buf[NAMELEN] = 0;
    scr_show(SCR_NAME);
    IN_ClearKeys();
    for (;;)
    {
        if (redraw)
        {
            ui_text(LAY_NAME_COL, LAY_NAME_ROW, buf, 0);
            redraw = 0;
        }
        wait_vblank();
        IN_Update();
        anim++;
        ui_pal(LAY_NAME_COL + pos, LAY_NAME_ROW, 1, 1, (anim >> 3) & 1);
        edge = pad_now & ~pad_last;
        if (edge & (PAD_UP | PAD_DOWN))
        {
            for (i = 0; i < n && name_chars[i] != buf[pos]; i++) ;
            if (i == n) i = 0;
            i = (edge & PAD_UP) ? (i + 1) % n : (i + n - 1) % n;
            buf[pos] = name_chars[i];
            redraw = 1;
        }
        if ((edge & PAD_RIGHT) && pos < NAMELEN - 1)
        {
            ui_pal(LAY_NAME_COL + pos, LAY_NAME_ROW, 1, 1, 0);
            pos++;
        }
        if ((edge & PAD_LEFT) && pos > 0)
        {
            ui_pal(LAY_NAME_COL + pos, LAY_NAME_ROW, 1, 1, 0);
            pos--;
        }
        if (pad_new & PAD_2)
        {
            buf[pos] = ' ';
            if (pos > 0) pos--;
            redraw = 1;
        }
        if (pad_new & (PAD_1 | PAD_PAUSE))
            break;
    }
    for (i = NAMELEN; i > 0 && buf[i - 1] == ' '; i--) ;
    buf[i] = 0;
    strcpy(name, buf);
    scr_leave();
}

void CheckHighScore(s32 score, u16 other) B
{
    DBG_SKIP();
    u8 i, j, rank = MAXSCORES;
    for (i = 0; i < MAXSCORES; i++)
        if (score > Scores[i].score)
        {
            rank = i;
            break;
        }
    if (rank < MAXSCORES)
    {
        for (j = MAXSCORES - 1; j > rank; j--)
            Scores[j] = Scores[j - 1];
        memset(Scores[rank].name, 0, NAMELEN + 1);
        Scores[rank].score = score;
        Scores[rank].completed = other;
        SD_StartMusic(ROSTER_MUS);
        name_entry(Scores[rank].name);
        if (!Scores[rank].name[0])
            strcpy(Scores[rank].name, "Anonymous");
        Scores_Store();
    }
    ViewScores();
}

void ViewScores(void) B
{
    SD_StartMusic(ROSTER_MUS);
    draw_scores();
    vid_fade_in();
    IN_ClearKeys();
    IN_Ack();
    scr_leave();
}

#ifdef DEBUG_MIRRORS
/* test hook: show a screen of the front end from inside a game (render.c dbg_uireq) */
void RestoreGameScreen(void) B;
void HelpScreens(void) B;
void dbg_ui_test(u8 which) B
{
    u8 i;
    dbg_noskip = 1;
    switch (which)
    {
    case 1:
        gamestate.mapon = 2;
        gamestate.TimeCount = 70L * 100;
        gamestate.killcount = 7; gamestate.killtotal = 10;
        gamestate.secretcount = 1; gamestate.secrettotal = 2;
        gamestate.treasurecount = 4; gamestate.treasuretotal = 4;
        gamestate.score = 12340;
        LevelCompleted();
        break;
    case 2:
        for (i = 0; i < 8; i++) { ratio_kill[i] = 60 + i; ratio_secret[i] = 30 + i; ratio_treasure[i] = 100; ratio_time[i] = 100 + i; }
        Victory();
        break;
    case 3: CheckHighScore(54321L, 3); break;
    case 4: EndText(); break;
    case 5: Inter_GetPsyched(); break;
    case 6: HelpScreens(); break;
    case 7: ViewScores(); break;
    }
    dbg_noskip = 0;
    RestoreGameScreen();
}
#endif

/* ------------------------------------------------------------ title loop */
void Quit_ToTitle(void) B
{
    __asm__("di\n jp 0");
}

void RestoreGameScreen(void) B
{
    vid_init_game();
    DrawStatusBar();
    ThreeDRefresh();
    vid_fade_in();
}

/* PG-13, then the title, credits and scores until a button is pressed; then the menu */
void title_loop(void) B
{
    u8 first = 1;
    ingame = FALSE;
    for (;;)
    {
        if (first)
        {
            scr_show(SCR_PG13);
            ui_wait(70 * 3);
            first = 0;
        }
        SD_StartMusic(NAZI_NOR_MUS);
        scr_show(SCR_TITLE);
        if (ui_wait(70 * 9)) goto menu;
        scr_show(SCR_CREDITS);
        if (ui_wait(70 * 6)) goto menu;
        draw_scores();
        vid_fade_in();
        if (ui_wait(70 * 6)) goto menu;
        continue;
menu:
        US_ControlPanel();
        if (startgame || loadedgame)
        {
            SD_MusicOff();
            GameLoop();
            ingame = FALSE;
            first = 0;
        }
    }
}
