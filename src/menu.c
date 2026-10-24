/* menu.c - the selection loop and the control panel (port of WL_MENU.C) (banked) */
#include "ui.h"
#include "snd.h"

extern u8 SoundOn, MusicOn, DigiOn;
void HelpScreens(void) B;
void ViewScores(void) B;
void RestoreGameScreen(void) B;

void ui_itoa(char *buf, s32 v) B
{
    char t[12];
    u8 n = 0, i = 0;
    if (v < 0) { buf[i++] = '-'; v = -v; }
    if (v == 0) t[n++] = '0';
    while (v) { t[n++] = '0' + (char)(v % 10); v /= 10; }
    while (n) buf[i++] = t[--n];
    buf[i] = 0;
}

void ui_pad(char *buf, s32 v, u8 w) B
{
    char t[12];
    u8 n, i;
    ui_itoa(t, v);
    for (n = 0; t[n]; n++) ;
    for (i = 0; i < w; i++)
        buf[i] = i + n < w ? ' ' : t[i + n - w];
    buf[w] = 0;
}

u8 ui_wait(u16 tics) B
{
    u32 t0 = get_timecount();
    IN_ClearKeys();
    do
    {
        wait_vblank();
        if (IN_CheckAck())
            return 1;
    } while (get_timecount() - t0 < tics);
    return 0;
}

static void hilite(const ScrItem *it, u8 on)
{
    ui_pal(it->col, it->row, it->w, it->h, on);
}

s8 menu_run(u8 start, void (*onmove)(u8)) B
{
    const ScrItem *it = (const ScrItem *)((const u8 *)scr_h + scr_h->items_off);
    u8 n = scr_h->nitems, cur = start, i, frame = 0, anim = 0, hold = 0, edge;
    s8 dir;
    for (i = 0; i < n && (it[cur].flags & 1); i++)
        cur = (cur + 1) % n;
    hilite(&it[cur], 1);
    ui_cursor(it[cur].cx, it[cur].cy, 0);
    if (onmove)
        onmove(cur);
    IN_ClearKeys();
    for (;;)
    {
        wait_vblank();
        IN_Update();
        anim++;
        if ((anim & 15) == 0)
        {
            frame ^= 1;
            ui_cursor(it[cur].cx, it[cur].cy, frame);
        }
        edge = pad_now & ~pad_last;
        dir = 0;
        if (edge & (PAD_UP | PAD_DOWN))
        {
            hold = 0;
            dir = (edge & PAD_UP) ? -1 : 1;
        }
        else if (pad_now & (PAD_UP | PAD_DOWN))
        {
            hold++;
            if (hold > 20 && ((hold - 20) & 3) == 0)
                dir = (pad_now & PAD_UP) ? -1 : 1;
        }
        else
            hold = 0;
        if (dir)
        {
            hilite(&it[cur], 0);
            i = n;
            do
            {
                cur = (u8)((s8)cur + dir + (s8)n) % n;
            } while (--i && (it[cur].flags & 1));
            hilite(&it[cur], 1);
            ui_cursor(it[cur].cx, it[cur].cy, frame);
            if (onmove)
                onmove(cur);
            SD_PlaySound(MOVEGUN2SND);
        }
        if (pad_new & PAD_1)
        {
            SD_PlaySound(SHOOTSND);
            return (s8)cur;
        }
        if (pad_new & (PAD_2 | PAD_PAUSE))
        {
            SD_PlaySound(ESCPRESSEDSND);
            return -1;
        }
    }
}

/* ------------------------------------------------------------ new game */
static void face(u8 i)
{
    ui_variant(0, i);
}

static u8 cp_new_game(void)
{
    s8 w;
    if (ingame)
    {
        scr_show(SCR_CONF_NEW);
        if (menu_run(1, 0) != 0)
            return 0;
    }
    scr_show(SCR_NEWGAME);
    w = menu_run(2, face);
    if (w < 0)
        return 0;
    NewGame(w, 0);
    startgame = TRUE;
    return 1;
}

/* --------------------------------------------------------------- sound */
static void sound_marks(void)
{
    ui_variant(0, SoundOn == 0);
    ui_variant(1, SoundOn != 0);
    ui_variant(2, DigiOn == 0);
    ui_variant(3, DigiOn == 1);
    ui_variant(4, DigiOn == 2);
    ui_variant(5, MusicOn == 0);
    ui_variant(6, MusicOn != 0);
}

static void cp_sound(void)
{
    s8 w;
    u8 pos = 0;
    scr_show(SCR_SOUND);
    sound_marks();
    for (;;)
    {
        w = menu_run(pos, 0);
        if (w < 0)
            return;
        pos = (u8)w;
        switch (w)
        {
        case 0: SoundOn = 0; SD_StopSound(); break;
        case 1: SoundOn = 1; SD_PlaySound(SHOOTSND); break;
        case 2: DigiOn = 0; SD_StopSound(); break;
        case 3: DigiOn = 1; SD_PlaySound(GUTENTAGSND); break;
        case 4: DigiOn = 2; SD_PlaySound(ATKPISTOLSND); break;
        case 5: if (MusicOn) { MusicOn = 0; SD_MusicOff(); } break;
        case 6: if (!MusicOn) { MusicOn = 1; SD_StartMusic(WONDERIN_MUS); } break;
        }
        sound_marks();
    }
}

/* ------------------------------------------------------- load and save */
static const char *const diffname[4] = { "BABY", "EASY", "NORMAL", "HARD" };

static void slot_text(u8 k)
{
    SaveInfo si;
    char b[32], n[12];
    u8 row = LAY_SLOT_ROW0 + 6 * k;
    ui_clear(LAY_SLOT_COL, row, LAY_SLOT_W, 2);
    if (!Save_Info(k, &si))
    {
        ui_text(LAY_SLOT_COL, row, "SLOT", 0);
        ui_itoa(n, k + 1);
        ui_text(LAY_SLOT_COL + 5, row, n, 0);
        ui_text(LAY_SLOT_COL + 8, row, "- EMPTY -", 0);
        return;
    }
    ui_text(LAY_SLOT_COL, row, "SLOT", 0);
    ui_itoa(n, k + 1);
    ui_text(LAY_SLOT_COL + 5, row, n, 0);
    ui_text(LAY_SLOT_COL + 8, row, "FLOOR", 0);
    ui_itoa(n, si.floor);
    ui_text(LAY_SLOT_COL + 14, row, n, 0);
    ui_text(LAY_SLOT_COL + 18, row, diffname[si.diff & 3], 0);
    ui_text(LAY_SLOT_COL, row + 1, "SCORE", 0);
    ui_pad(b, si.score, 8);
    ui_text(LAY_SLOT_COL + 6, row + 1, b, 0);
    ui_text(LAY_SLOT_COL + 16, row + 1, "LIVES", 0);
    ui_itoa(n, si.lives);
    ui_text(LAY_SLOT_COL + 22, row + 1, n, 0);
}

static void slots_draw(u8 id)
{
    u8 k;
    scr_show(id);
    for (k = 0; k < 3; k++)
        slot_text(k);
}

static u8 cp_load(void)
{
    s8 w;
    SaveInfo si;
    slots_draw(SCR_LOAD);
    for (;;)
    {
        w = menu_run(0, 0);
        if (w < 0)
            return 0;
        if (!Save_Info((u8)w, &si))
            continue;
        scr_leave();                    /* slot 2 back: the level is built in cartridge RAM */
        if (Save_Read((u8)w))
        {
            loadedgame = TRUE;
            return 1;
        }
        slots_draw(SCR_LOAD);
        ui_text(LAY_SLOT_COL, LAY_SLOT_ROW0 + 2, "CAN'T LOAD THIS GAME", 1);
    }
}

static void cp_save(void)
{
    s8 w;
    slots_draw(SCR_SAVE);
    for (;;)
    {
        w = menu_run(0, 0);
        if (w < 0)
            return;
        ui_text(LAY_SLOT_COL, LAY_SLOT_ROW0 + 6 * w, "SAVING...                ", 1);
        ui_cursor_off();
        {
            u8 old = slot2_cur, ok;
            slot2_map(SLOT2_CARTRAM);   /* the level */
            ok = Save_Write((u8)w);
            slot2_map(old);
            slot_text((u8)w);
            ui_pal(LAY_SLOT_COL, LAY_SLOT_ROW0 + 6 * w, LAY_SLOT_W, 2, 1);
            if (!ok)
                ui_text(LAY_SLOT_COL, LAY_SLOT_ROW0 + 6 * w + 1, "NO ROOM - NOT SAVED", 1);
            else
                ui_wait(35);
        }
        ui_cursor(12, 64 + 48 * w, 0);
    }
}

/* ---------------------------------------------------- read this, the end */
static void show_pages(u8 first, u8 count)
{
    s8 page = 0;
    u8 edge;
    scr_show(first);
    IN_ClearKeys();
    for (;;)
    {
        wait_vblank();
        IN_Update();
        edge = pad_now & ~pad_last;
        if (edge & (PAD_UP | PAD_LEFT))
        {
            if (page > 0)
                scr_show(first + --page);
        }
        else if ((edge & (PAD_DOWN | PAD_RIGHT)) || (pad_new & PAD_1))
        {
            if (page < count - 1)
                scr_show(first + ++page);
            else if (pad_new & PAD_1)
                break;
        }
        if (pad_new & (PAD_2 | PAD_PAUSE))
            break;
    }
}

void HelpScreens(void) B
{
    show_pages(SCR_HELP_FIRST, HELP_PAGES);
}

void EndText(void) B
{
    show_pages(SCR_END_FIRST, END_PAGES);
    scr_leave();
}

/* --------------------------------------------------------- control panel */
void US_ControlPanel(void) B
{
    s8 w;
    u8 pos = 0, done = 0;
    SD_StartMusic(WONDERIN_MUS);
    for (;;)
    {
        scr_show(ingame ? SCR_MAIN_GAME : SCR_MAIN_TITLE);
        w = menu_run(pos, 0);
        if (w < 0 || w == 6)
            break;
        pos = (u8)w;
        switch (w)
        {
        case 0: if (cp_new_game()) done = 1; break;
        case 1: cp_sound(); break;
        case 2: if (cp_load()) done = 1; break;
        case 3: cp_save(); break;
        case 4: HelpScreens(); break;
        case 5: ViewScores(); break;
        case 7:
            scr_show(SCR_CONF_QUIT);
            if (menu_run(1, 0) == 0)
                Quit_ToTitle();
            break;
        }
        if (done)
            break;
    }
    scr_leave();
    if (ingame && !done)
        RestoreGameScreen();
}
