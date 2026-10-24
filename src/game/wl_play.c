#include "def.h"
/* wl_play.c - the play loop, actor list and controls (port of WL_PLAY.C) */
#include <string.h>

boolean madenoise;
exit_t playstate;
objtype *newobj, *obj, *player, *lastobj, *objfreelist, *killerobj;
boolean singlestep, godmode, noclip;
s16 extravbls;


s16 tics;
s32 lasttimecount;
boolean fizzlein;
boolean startgame, loadedgame;

s16 controlx, controly;
boolean buttonstate[NUMBUTTONS];
boolean buttonheld[NUMBUTTONS];
s16 shootdelta = VIEWW / 10;

/* songs of the first episode, indexed by level */
const s16 songs[10] =
{
    GETTHEM_MUS, SEARCHN_MUS, POW_MUS, SUSPENSE_MUS,
    GETTHEM_MUS, SEARCHN_MUS, POW_MUS, SUSPENSE_MUS,
    WARMARCH_MUS, CORNER_MUS
};

#define BASEMOVE    35
#define RUNMOVE     70
#define MAXTICS     10

/* ---------------------------------------------------------- controls */

#define BASEMOVE    35
#define RUNMOVE     70

static s16 ctlx, ctly;          /* control per tic: -35..35 */

/* pad: 1 fire, 2 use (+ LEFT / RIGHT: strafe), 2 + DOWN: next weapon */
static void CycleWeapon(void)
{
    s16 w = gamestate.chosenweapon, n;
    if (!gamestate.ammo)
        return;
    for (n = 0; n < 4; n++)
    {
        w++;
        if (w > gamestate.bestweapon) w = wp_knife;
        gamestate.weapon = gamestate.chosenweapon = (weapontype)w;
        DrawWeapon();
        SD_PlaySound(SELECTWPNSND);
        return;
    }
}

void CalcTics (void) B
{
    s32 newtime;

#ifdef DEBUG_DET
    tics = 24;                      /* deterministic build (make DET=1): a fixed time step, no waiting */
    return;
#endif
    if (lasttimecount > (s32)get_timecount())
        set_timecount(lasttimecount);
    do
    {
        newtime = get_timecount();
        tics = newtime - lasttimecount;
        if (!tics)
            wait_vblank();
    } while (!tics);
    lasttimecount = newtime;
    /* the play loop runs a frame in steps of at most MAXTICS; slower than
     * 3 steps the game slows down */
    if (tics > MAXTICS * 3)
    {
        set_timecount(get_timecount() - (tics - MAXTICS * 3));
        tics = MAXTICS * 3;
    }
}

/* once per frame: the buttons and the movement per tic (PlayLoop scales
 * the movement by the length of each step) */
void PollControls (void) B
{
    CalcTics();

    memcpy(buttonheld, buttonstate, sizeof(buttonstate));
    memset(buttonstate, 0, sizeof(buttonstate));

    IN_Update();
#ifdef DEBUG_DET
    pad_new = pad_now & ~pad_last;  /* the pad as it is at this poll: independent of the speed of the build */
    pad_f_up = (pad_now & PAD_UP) ? 256 : 0;
    pad_f_down = (pad_now & PAD_DOWN) ? 256 : 0;
    pad_f_left = (pad_now & PAD_LEFT) ? 256 : 0;
    pad_f_right = (pad_now & PAD_RIGHT) ? 256 : 0;
#endif
    ctlx = ctly = 0;
    /* a frame lasts 0.2 - 0.4 s: a press that began since the last poll is a fresh one even when
     * the button looked down then, and a direction moves for as long as it was down */
    if (pad_now & PAD_1) buttonstate[bt_attack] = TRUE;
    if (pad_new & PAD_1) buttonheld[bt_attack] = FALSE;
    if (pad_now & PAD_2)
    {
        buttonstate[bt_strafe] = (pad_now & (PAD_LEFT | PAD_RIGHT)) != 0;
        buttonstate[bt_use] = TRUE;
        if (pad_new & PAD_2) buttonheld[bt_use] = FALSE;
        if ((pad_now & PAD_DOWN) && ((pad_new & PAD_2) || !(pad_last & PAD_2)))
        {
            CycleWeapon();
            buttonstate[bt_use] = FALSE;
            buttonheld[bt_use] = TRUE;
        }
    }
    ctly = (s16)(((s32)BASEMOVE * ((s16)pad_f_down - (s16)pad_f_up)) >> 8);
    ctlx = (s16)(((s32)BASEMOVE * ((s16)pad_f_right - (s16)pad_f_left)) >> 8);
}

static void StepControls(void)
{
    controlx = ctlx * tics;
    controly = ctly * tics;
}

/* ------------------------------------------------------------ actors */

static s16 objcount;

void InitActorList (void) B
{
    s16 i;

    for (i = 0; i < MAXACTORS; i++)
    {
        objlist[i].prev = &objlist[i + 1];
        objlist[i].next = NULL;
        objlist[i].enc = 0x40 | (i & 0x3F) | ((i & 0x40) << 1);
    }
    objlist[MAXACTORS - 1].prev = NULL;

    objfreelist = &objlist[0];
    lastobj = NULL;
    objcount = 0;

    GetNewActor();
    player = newobj;
}

void GetNewActor (void) B
{
    if (!objfreelist)
        Quit("GetNewActor: No free spots in objlist!");

    newobj = objfreelist;
    objfreelist = newobj->prev;
    {
        u8 enc = newobj->enc;
        memset(newobj, 0, sizeof(*newobj));
        newobj->enc = enc;
    }

    if (lastobj)
        lastobj->next = newobj;
    newobj->prev = lastobj;
    newobj->active = FALSE;
    lastobj = newobj;
    objcount++;
}

void RemoveObj (objtype *gone) B
{
    if (gone == player)
        Quit("RemoveObj: Tried to remove the player!");

    gone->state = NULL;

    if (gone == lastobj)
        lastobj = (objtype *)gone->prev;
    else
        gone->next->prev = gone->prev;

    gone->prev->next = gone->next;

    gone->prev = objfreelist;
    objfreelist = gone;
    objcount--;
}

/* ------------------------------------------------------- palette shifts */

#define NUMREDSHIFTS    6
#define REDSTEPS        8
#define NUMWHITESHIFTS  3
#define WHITESTEPS      20
#define WHITETICS       6

s16 damagecount, bonuscount;      /* (global: smstest prints them) */
static boolean palshifted;

void ClearPaletteShifts (void) B
{
    bonuscount = damagecount = 0;
}

void StartBonusFlash (void) B
{
    bonuscount = NUMWHITESHIFTS * WHITETICS;
}

void StartDamageFlash (s16 damage) B
{
    damagecount += damage;
}

/* the SMS palette has 4 levels per channel: the original's shift steps 1..levels become
 * 0..maxlevel (a full level 3 would paint the whole screen one colour) */
static void ApplyShift(s16 r, s16 g, s16 b, u16 level, u16 levels, u16 maxlevel)
{
    vid_flash(r ? 1 : 0, g ? 1 : 0, b ? 1 : 0, (u8)((level * maxlevel + levels / 2) / levels));
}

void UpdatePaletteShifts (void) B
{
    s16 red, white;

    if (bonuscount)
    {
        white = bonuscount / WHITETICS + 1;
        if (white > NUMWHITESHIFTS) white = NUMWHITESHIFTS;
        bonuscount -= tics;
        if (bonuscount < 0) bonuscount = 0;
    }
    else
        white = 0;

    if (damagecount)
    {
        red = damagecount / 10 + 1;
        if (red > NUMREDSHIFTS) red = NUMREDSHIFTS;
        damagecount -= tics;
        if (damagecount < 0) damagecount = 0;
    }
    else
        red = 0;

    if (red)
    {
        ApplyShift(7, 0, 0, red, NUMREDSHIFTS, 2);
        palshifted = TRUE;
    }
    else if (white)
    {
        ApplyShift(7, 7, 0, white, NUMWHITESHIFTS, 1);
        palshifted = TRUE;
    }
    else if (palshifted)
    {
        vid_flash(0, 0, 0, 0);
        palshifted = FALSE;
    }
}

void FinishPaletteShifts (void) B
{
    if (palshifted)
    {
        palshifted = FALSE;
        vid_flash(0, 0, 0, 0);
    }
}

/* ------------------------------------------------------------- actors */

static void DoActor(objtype *ob)
{
    u8 think;

    if (!ob->active && !areabyplayer[ob->areanumber])
        return;

    if (!(ob->flags & (FL_NONMARK | FL_NEVERMARK)))
        actorat[ob->tilex][ob->tiley] = 0;

    if (!ob->ticcount)
    {
        think = ob->state->think;
        if (think)
        {
            think_call(think, ob);
            if (!ob->state)
            {
                RemoveObj(ob);
                return;
            }
        }

        if (ob->flags & FL_NEVERMARK)
            return;

        if ((ob->flags & FL_NONMARK) && actorat[ob->tilex][ob->tiley])
            return;

        actorat[ob->tilex][ob->tiley] = act_enc(ob);
        return;
    }

    ob->ticcount -= tics;
    while (ob->ticcount <= 0)
    {
        think = ob->state->action;
        if (think)
        {
            action_call(think, ob);
            if (!ob->state)
            {
                RemoveObj(ob);
                return;
            }
        }

        ob->state = ob->state->next;

        if (!ob->state)
        {
            RemoveObj(ob);
            return;
        }

        if (!ob->state->tictime)
        {
            ob->ticcount = 0;
            goto dothink;
        }

        ob->ticcount += ob->state->tictime;
    }

dothink:
    think = ob->state->think;
    if (think)
    {
        think_call(think, ob);
        if (!ob->state)
        {
            RemoveObj(ob);
            return;
        }
    }

    if (ob->flags & FL_NEVERMARK)
        return;

    if ((ob->flags & FL_NONMARK) && actorat[ob->tilex][ob->tiley])
        return;

    actorat[ob->tilex][ob->tiley] = act_enc(ob);
}

/* --------------------------------------------------------------- music */

void StartMusic (void) B
{
    SD_StartMusic(songs[gamestate.mapon % 10]);
}

void StopMusic (void) B
{
    SD_MusicOff();
}

/* ---------------------------------------------------------- play loop */

static void CheckKeys(void)
{
    if (pad_now & PAD_PAUSE)
    {
        StopMusic();
        FinishPaletteShifts();
        US_ControlPanel();
        if (!startgame && !loadedgame)
            StartMusic();
        if (loadedgame)
            playstate = ex_abort;
        lasttimecount = get_timecount();
        IN_ClearKeys();
    }
}

void PlayLoop (void) B
{
    s16 left, step, total;

    playstate = ex_stillplaying;
    set_timecount(0);
    lasttimecount = 0;
    running = FALSE;
    anglefrac = 0;
    facecount = 0;
    memset(buttonstate, 0, sizeof(buttonstate));
    ClearPaletteShifts();
    IN_ClearKeys();

    do
    {
        PollControls();

        total = left = tics;
        while (left > 0 && !playstate && !startgame)
        {
            step = left > MAXTICS ? MAXTICS : left;
            left -= step;
            tics = step;
            StepControls();
            madenoise = FALSE;
            MoveDoors();
            MovePWalls();

            for (obj = player; obj; obj = obj->next)
                DoActor(obj);
            gamestate.TimeCount += tics;
        }
        tics = total;               /* the whole frame: the palette shifts decay by it */

        UpdatePaletteShifts();

        ThreeDRefresh();

        UpdateSoundLoc();

        CheckKeys();
    } while (!playstate && !startgame);

    if (playstate != ex_died)
        FinishPaletteShifts();
}
