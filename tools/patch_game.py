#!/usr/bin/env python3
"""Hand edits of the ported sources (applied right after port_game.py -f).
Every edit asserts that its anchor text exists exactly once."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
G = os.path.join(HERE, '..', 'src', 'game')

cur = {}


def load(f):
    cur['f'] = f
    cur['s'] = open(os.path.join(G, f)).read()


def save():
    open(os.path.join(G, cur['f']), 'w').write(cur['s'])


def rep(old, new, count=1):
    n = cur['s'].count(old)
    assert n == count, (cur['f'], old[:70], n)
    cur['s'] = cur['s'].replace(old, new)


def between(start, end, new, keep_end=True):
    s = cur['s']
    a = s.index(start)
    b = s.index(end, a)
    cur['s'] = s[:a] + new + (s[b:] if keep_end else s[b + len(end):])


def strip_cases(names):
    """remove `case X:` blocks of the given labels from every switch"""
    lines = cur['s'].split('\n')
    out = []
    i = 0
    while i < len(lines):
        m = re.match(r'^(\s*)case (\w+):\s*$', lines[i])
        if m and m.group(2) in names:
            ind = m.group(1)
            nxt = lines[i + 1] if i + 1 < len(lines) else ''
            if re.match(r'^%scase \w+:' % ind, nxt):
                i += 1                    # stacked labels: drop just this one
                continue
            i += 1
            while i < len(lines):
                l = lines[i]
                if l.strip() == '':
                    i += 1
                    continue
                cind = len(l) - len(l.lstrip())
                if cind < len(ind) or (cind == len(ind) and re.match(r'\s*(case\b|default\b|\})', l)):
                    break
                i += 1
            continue
        out.append(lines[i])
        i += 1
    cur['s'] = '\n'.join(out)


GONE = ['officerobj', 'mutantobj', 'gretelobj', 'giftobj', 'fatobj', 'schabbobj', 'fakeobj',
        'mechahitlerobj', 'realhitlerobj', 'ghostobj', 'rocketobj', 'needleobj', 'fireobj',
        'spectreobj', 'angelobj', 'transobj', 'uberobj', 'willobj', 'deathobj', 'hrocketobj', 'sparkobj',
        'en_officer', 'en_mutant', 'en_schabbs', 'en_fake', 'en_hitler', 'en_blinky', 'en_clyde',
        'en_pinky', 'en_inky', 'en_gretel', 'en_gift', 'en_fat', 'en_spectre', 'en_angel', 'en_trans',
        'en_uber', 'en_will', 'en_death']


def p_play():
    # ------------------------------------------------------------ wl_play.c
    load('wl_play.c')
    rep('objtype objlist[MAXACTORS];\n', '')
    rep('u8 tilemap[MAPSIZE][MAPSIZE];\nu8 actorat[MAPSIZE][MAPSIZE];\n', '')
    between('/* ---------------------------------------------------------- controls */\n',
            '/* ------------------------------------------------------------ actors */\n', '''/* ---------------------------------------------------------- controls */

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

''')
    # every slot of the object list carries its actorat code (objtype.enc): no division by sizeof(objtype)
    rep("""        objlist[i].prev = &objlist[i + 1];
        objlist[i].next = NULL;
    }""", """        objlist[i].prev = &objlist[i + 1];
        objlist[i].next = NULL;
        objlist[i].enc = 0x40 | (i & 0x3F) | ((i & 0x40) << 1);
    }""")
    rep("""    memset(newobj, 0, sizeof(*newobj));""", """    {
        u8 enc = newobj->enc;
        memset(newobj, 0, sizeof(*newobj));
        newobj->enc = enc;
    }""")
    rep('static void DoActor(objtype *ob)\n{\n    void (*think)(objtype *);\n', 'static void DoActor(objtype *ob)\n{\n    u8 think;\n')
    rep('        think = ob->state->think;\n        if (think)\n        {\n            think(ob);', '        think = ob->state->think;\n        if (think)\n        {\n            think_call(think, ob);')
    rep('        think = ob->state->action;\n        if (think)\n        {\n            think(ob);', '        think = ob->state->action;\n        if (think)\n        {\n            action_call(think, ob);')
    rep('    think = ob->state->think;\n    if (think)\n    {\n        think(ob);', '    think = ob->state->think;\n    if (think)\n    {\n        think_call(think, ob);')
    # palette shifts
    rep('static s16 damagecount, bonuscount;', 's16 damagecount, bonuscount;      /* (global: smstest prints them) */')
    between('static void ApplyShift(', 'void UpdatePaletteShifts (void) B', '''/* the SMS palette has 4 levels per channel: the original's shift steps 1..levels become
 * 0..maxlevel (a full level 3 would paint the whole screen one colour) */
static void ApplyShift(s16 r, s16 g, s16 b, u16 level, u16 levels, u16 maxlevel)
{
    vid_flash(r ? 1 : 0, g ? 1 : 0, b ? 1 : 0, (u8)((level * maxlevel + levels / 2) / levels));
}

''')
    rep('        ApplyShift(7, 7, 0, white, WHITESTEPS);', '        ApplyShift(7, 7, 0, white, NUMWHITESHIFTS, 1);')
    rep('        ApplyShift(7, 0, 0, red, REDSTEPS);', '        ApplyShift(7, 0, 0, red, NUMREDSHIFTS, 2);')
    rep('''    else if (palshifted)
    {
        vid_set_pal(0, game_pal0);
        vid_set_pal(1, game_pal1);
        palshifted = FALSE;
    }''', '''    else if (palshifted)
    {
        vid_flash(0, 0, 0, 0);
        palshifted = FALSE;
    }''')
    rep('''    if (palshifted)
    {
        palshifted = FALSE;
        vid_set_pal(0, game_pal0);
        vid_set_pal(1, game_pal1);
    }''', '''    if (palshifted)
    {
        palshifted = FALSE;
        vid_flash(0, 0, 0, 0);
    }''')
    rep('extern u16 game_pal0[16];\nextern u16 game_pal1[16];\n', '')
    rep('    SD_StartLevelMusic(songs[gamestate.mapon % 10]);', '    SD_StartMusic(songs[gamestate.mapon % 10]);')
    rep('    if (in_pressed & BUTTON_START)', '    if (pad_now & PAD_PAUSE)')
    rep('        lasttimecount = TimeCount;\n        IN_ClearKeys();', '        lasttimecount = get_timecount();\n        IN_ClearKeys();')
    # play loop: steps of at most MAXTICS
    between('    playstate = ex_stillplaying;\n    TimeCount = lasttimecount = 0;', '    if (playstate != ex_died)\n        FinishPaletteShifts();', '''    s16 left, step, total;

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

''')
    rep('void PlayLoop (void) B\n{\n', 'void PlayLoop (void) B\n{\n')
    save()
    print('wl_play.c patched')



def p_agent():
    load('wl_agent.c')
    between('void DrawFace (void) B\n{', '/*\n===============\n=\n= UpdateFace', '''void DrawFace (void) B
{
\tif (gamestate.health)
\t\thud_face(3*((100-gamestate.health)/16)+gamestate.faceframe);
\telse
\t\thud_face(FACE_DEAD);
}


''')
    between('/*\n===============\n=\n= LatchNumber', '/*\n===============\n=\n= DrawHealth', '')
    rep('\tLatchNumber (21,16,3,gamestate.health);', '\thud_number(HUD_HEALTH, gamestate.health);')
    rep('\tLatchNumber (2,16,2,gamestate.mapon+1);', '\thud_number(HUD_FLOOR, gamestate.mapon+1);')
    rep('\tLatchNumber (14,16,1,gamestate.lives);', '\thud_number(HUD_LIVES, gamestate.lives);')
    rep('\tLatchNumber (6,16,6,gamestate.score);', '\thud_number(HUD_SCORE, gamestate.score);')
    rep('\tLatchNumber (27,16,2,gamestate.ammo);', '\thud_number(HUD_AMMO, gamestate.ammo);')
    rep('\tStatusDrawPic (32,8,KNIFEPIC+gamestate.weapon);', '\thud_weapon(gamestate.weapon);\n\tweapon_show(gamestate.weapon, gamestate.weaponframe);')
    between('void DrawKeys (void) B\n{', '/*\n==================\n=\n= GiveWeapon', 'void DrawKeys (void) B\n{\n\thud_keys(gamestate.keys);\n}\n\n\n\n')
    rep('\tswitch (check->itemnumber)', '\tswitch (statinfo_type[check->type])')
    rep('\t\tGiveKey (check->itemnumber - bo_key1);', '\t\tGiveKey (statinfo_type[check->type] - bo_key1);')
    rep('\t\tStatusDrawPic (17,4,GOTGATLINGPIC);', '\t\thud_face(FACE_GATLING);')
    rep('\tcheck->shapenum = -1;\t\t\t// remove from list', '\tcheck->type = STAT_REMOVED;\t\t// remove from list')
    rep('\tplayer->areanumber = MAPSPOT0(player->tilex, player->tiley);', '\tplayer->areanumber = AREA(player->tilex, player->tiley);')
    # pickups.  The original grabs a bonus static from the drawing code (DrawScaleds ->
    # TransformTile -> GetBonus) while it is within a tile ahead of the viewer; this port's
    # frames are long enough to step over it, so every movement step checks the player's tile
    rep('\tcheck->type = STAT_REMOVED;\t\t// remove from list\n}', '''\tcheck->type = STAT_REMOVED;\t\t// remove from list
}

/* takes the bonus objects on the player's tile (called after every move, see Thrust).  The statics
 * never move: the list is only scanned when the tile changed, when a static was added (a dropped item,
 * a new level: pick_valid = 0) or when a bonus was left behind (full health...) */
u8 pick_valid;
static u8 pick_x, pick_y;
u8 exit_valid;                          /* Thrust: the exit tile of the player's tile is known */
static u8 exit_x, exit_y;
void CheckPickups (void) B
{
\tstatobj_t *st;
\tu8 tx = player->tilex, ty = player->tiley;

\tif (pick_valid && tx == pick_x && ty == pick_y)
\t\treturn;
\tpick_x = tx;
\tpick_y = ty;
\tpick_valid = 1;
\tfor (st = statobjlist; st != laststatobj; st++)
\t\tif (st->tilex == tx && st->tiley == ty && st->type != STAT_REMOVED
\t\t&& statinfo_type[st->type] >= bo_gibs)
\t\t{
\t\t\tGetBonus (st);
\t\t\tif (st->type != STAT_REMOVED)
\t\t\t\tpick_valid = 0;\t\t// left behind: look again
\t\t}
}''')
    # the exit (elevator) tile: looked up when the player stands on another tile than at the last look
    rep('\tif (InfoPlane(player->tilex, player->tiley) == EXITTILE)\n\t\tVictoryTile ();\n}', '''\tCheckPickups ();

\tif (player->tilex != exit_x || player->tiley != exit_y || !exit_valid)
\t{
\t\texit_x = player->tilex;
\t\texit_y = player->tiley;
\t\texit_valid = 1;
\t\tif (InfoPlane(player->tilex, player->tiley) == EXITTILE)
\t\t\tVictoryTile ();
\t}
}''')
    save()
    print('wl_agent.c patched')



def p_act1():
    load('wl_act1.c')
    s = cur['s']
    a = s.index('statobj_t statobjlist')
    b = s.index('/*\n * DOORS\n */')
    new_top = '''statobj_t *laststatobj;

/* the info plane lives in ROM (InfoPlane); the few tiles the game clears at
 * run time (a pushed secret wall loses its marker) are remembered in a short
 * list */
#define MAXCLEARED 32
u8 info_clr_x[MAXCLEARED], info_clr_y[MAXCLEARED];
u8 info_clr_n;

static void ClearInfo(s16 x, s16 y)
{
    if (info_clr_n < MAXCLEARED)
    {
        info_clr_x[info_clr_n] = x;
        info_clr_y[info_clr_n] = y;
        info_clr_n++;
    }
}

void InitStaticList (void) B
{
    laststatobj = &statobjlist[0];
    info_clr_n = 0;
    STATICS_CHANGED();                  /* CheckPickups and the sprite list: scan again */
}

void SpawnStatic (s16 tilex, s16 tiley, s16 type) B
{
    STATICS_CHANGED();
    laststatobj->tilex = tilex;
    laststatobj->tiley = tiley;
    laststatobj->type = type;

    switch (statinfo_type[type])
    {
    case block:
        actorat[tilex][tiley] = 1;      /* consider it a blocking tile */
    case dressing:
        break;

    case bo_cross:
    case bo_chalice:
    case bo_bible:
    case bo_crown:
    case bo_fullheal:
        if (!loadedgame)
            gamestate.treasuretotal++;
    default:
        break;
    }

    laststatobj++;
    if (laststatobj == &statobjlist[MAXSTATS])
        Quit("Too many static objects!");
}

/* Called during game play to drop actors' items. */
void PlaceItemType (s16 itemtype, s16 tilex, s16 tiley) B
{
    s16 type;
    statobj_t *spot;

    STATICS_CHANGED();
    for (type = 0;; type++)
    {
        if (type == NUMSTATTYPES)
            Quit("PlaceItemType: couldn't find type!");
        if (statinfo_type[type] == itemtype)
            break;
    }

    for (spot = &statobjlist[0];; spot++)
    {
        if (spot == laststatobj)
        {
            if (spot == &statobjlist[MAXSTATS])
                return;
            laststatobj++;
            break;
        }
        if (spot->type == STAT_REMOVED)
            break;
    }

    spot->type = type;
    spot->tilex = tilex;
    spot->tiley = tiley;
}

'''
    s = s[:a] + new_top + s[b:]
    a = s.index('doorobj_t doorobjlist[MAXDOORS], *lastdoorobj;')
    b = s.index('void OpenDoor (s16 door) B')
    new_doors = '''doorobj_t *lastdoorobj;
s16 doornum;
u8 doors_busy;                          /* doors that are not closed: MoveDoors has nothing to do at 0 */
boolean areabyplayer[NUMAREAS];

/* the areas the player can hear / see into: through every door that is not
 * fully closed */
void ConnectAreas (void) B
{
    u8 d, a, b, changed;
    memset(areabyplayer, 0, sizeof(areabyplayer));
    areabyplayer[player->areanumber] = TRUE;
    do
    {
        changed = 0;
        for (d = 0; d < doornum; d++)
        {
            if (doorobjlist[d].action == dr_closed)
                continue;
            a = doorobjlist[d].area1;
            b = doorobjlist[d].area2;
            if (areabyplayer[a] != areabyplayer[b])
            {
                areabyplayer[a] = areabyplayer[b] = TRUE;
                changed = 1;
            }
        }
    } while (changed);
}

void InitAreas (void) B
{
    memset(areabyplayer, 0, sizeof(areabyplayer));
    areabyplayer[player->areanumber] = TRUE;
}

void InitDoorList (void) B
{
    memset(areabyplayer, 0, sizeof(areabyplayer));
    lastdoorobj = &doorobjlist[0];
    doornum = 0;
    doors_busy = 0;
}

/* the tile map and the door table come ready made from the level blob */
void SpawnDoor (u8 tilex, u8 tiley, u8 vertical, u8 lock, u8 a1, u8 a2) B
{
    if (doornum == 64)
        Quit("64+ doors on level!");

    doorposition[doornum] = 0;
    ((u8 *)CR_DOORPOS)[doornum] = 0;
    ((u8 *)CR_DOORPAGE)[doornum] = lock == dr_elevator ? DOORW_ELEV : lock ? DOORW_LOCK : DOORW_NORMAL;
    lastdoorobj->tilex = tilex;
    lastdoorobj->tiley = tiley;
    lastdoorobj->vertical = vertical;
    lastdoorobj->lock = lock;
    lastdoorobj->action = dr_closed;
    lastdoorobj->area1 = a1;
    lastdoorobj->area2 = a2;
    lastdoorobj->ticcount = 0;

    actorat[tilex][tiley] = doornum | 0x80;

    doornum++;
    lastdoorobj++;
}

'''
    s = s[:a] + new_doors + s[b:]
    cur['s'] = s
    rep('    area = MAPSPOT0(doorobjlist[door].tilex, doorobjlist[door].tiley) - AREATILE;\n    if (areabyplayer[area])', '    area = doorobjlist[door].area1;\n    if (areabyplayer[area])')
    between('static void DoorOpening(s16 door)', '    position += (s32)tics << 10;', '''static void DoorOpening(s16 door)
{
    s32 position;

    position = doorposition[door];
    if (!position)
    {
        ConnectAreas();

        if (areabyplayer[doorobjlist[door].area1])
            PlaySoundLocTile(OPENDOORSND, doorobjlist[door].tilex, doorobjlist[door].tiley);
    }

''')
    rep('''        actorat[doorobjlist[door].tilex][doorobjlist[door].tiley] = 0;
    }

    doorposition[door] = position;
}''', '''        actorat[doorobjlist[door].tilex][doorobjlist[door].tiley] = 0;
    }

    doorposition[door] = position;
    ((u8 *)CR_DOORPOS)[door] = position >> 8;
    render_cast_dirty = 1;
}''')
    between('static void DoorClosing(s16 door)', 'void MoveDoors (void) B', '''static void DoorClosing(s16 door)
{
    s32 position;
    s16 tilex, tiley;

    tilex = doorobjlist[door].tilex;
    tiley = doorobjlist[door].tiley;

    if (actorat[tilex][tiley] != (door | 0x80)
        || (player->tilex == tilex && player->tiley == tiley))
    {
        OpenDoor(door);
        return;
    }

    position = doorposition[door];
    position -= (s32)tics << 10;
    if (position <= 0)
    {
        position = 0;
        doorobjlist[door].action = dr_closed;
        doors_busy--;
        ConnectAreas();
    }

    doorposition[door] = position;
    ((u8 *)CR_DOORPOS)[door] = position >> 8;
    render_cast_dirty = 1;
}

''')
    rep('        MAPSPOT0(pwallx, pwally) = player->areanumber + AREATILE;\n', '        SetAreaOverride(pwallx, pwally, player->areanumber);\n')
    rep('    pwallpos = (pwallstate / 2) & 63;\n}', '    pwallpos = (pwallstate / 2) & 63;\n    cr_pwpos = (u8)pwallpos;\n    render_cast_dirty = 1;\n}')
    rep('    tilemap[pwallx][pwally] |= 0xc0;\n    ClearInfo(pwallx, pwally);', '    tilemap[pwallx][pwally] |= 0xc0;\n    render_cast_dirty = 1;\n    ClearInfo(pwallx, pwally);')
    rep("""void OpenDoor (s16 door) B
{
    if (doorobjlist[door].action == dr_open)
        doorobjlist[door].ticcount = 0;
    else
        doorobjlist[door].action = dr_opening;
}""", """void OpenDoor (s16 door) B
{
    if (doorobjlist[door].action == dr_open)
        doorobjlist[door].ticcount = 0;
    else
    {
        if (doorobjlist[door].action == dr_closed)
            doors_busy++;
        doorobjlist[door].action = dr_opening;
    }
}""")
    rep("""    if (gamestate.victoryflag)
        return;

    for (door = 0; door < doornum; door++)""", """    if (gamestate.victoryflag || !doors_busy)
        return;

    for (door = 0; door < doornum; door++)""")
    save()
    print('wl_act1.c patched')


def p_state():
    load('wl_state.c')
    strip_cases(GONE)
    # CheckLine: ((s32)delta << 8) / deltafrac limited to +-0x7fff is a 24 / 16 bit division
    rep('''\t\tltemp = ((s32)delta<<8)/deltafrac;
\t\tif (ltemp > 0x7fffl)
\t\t\tystep = 0x7fff;
\t\telse if (ltemp < -0x7fffl)
\t\t\tystep = -0x7fff;
\t\telse
\t\t\tystep = ltemp;''', '\t\tystep = LineStep (delta, deltafrac);')
    rep('''\t\tltemp = ((s32)delta<<8)/deltafrac;
\t\tif (ltemp > 0x7fffl)
\t\t\txstep = 0x7fff;
\t\telse if (ltemp < -0x7fffl)
\t\t\txstep = -0x7fff;
\t\telse
\t\t\txstep = ltemp;''', '\t\txstep = LineStep (delta, deltafrac);')
    rep('boolean CheckLine (objtype *ob) B\n{', '''/* ((s32)delta << 8) / deltafrac limited to +-0x7fff, deltafrac > 0: a 24 by 16 bit division (the
 * library's 32 bit one costs 11000 cycles) */
s16 LineStep (s16 delta, s16 deltafrac) B
{
\tu32 n;
\tu16 q;
\tu8 neg = delta < 0;

\tn = (u32)(neg ? -delta : delta) << 8;
\tif ((u16)(n >> 16) >= (u16)deltafrac)
\t\treturn neg ? -0x7fff : 0x7fff;\t\t// the quotient has more than 16 bits
\tq = udiv32_16 (n, (u16)deltafrac);
\tif (q > 0x7fff)
\t\tq = 0x7fff;
\treturn neg ? -(s16)q : (s16)q;
}

boolean CheckLine (objtype *ob) B
{''')
    rep('\tnewobj->areanumber =\n\t\tMAPSPOT0(newobj->tilex, newobj->tiley) - AREATILE;', '\tnewobj->areanumber = AREA(newobj->tilex, newobj->tiley);')
    save()
    print('wl_state.c patched')


def p_act2():
    load('wl_act2.c')
    strip_cases(GONE)
    rep('\tmove = ob->speed*tics;', '\tmove = mul_tics ((u8)tics, (u16)ob->speed);', 3)
    between('/* angle (0..ANGLES-1, counter clockwise, y up) of the vector (dx,dy) */', 'dirtype dirtable[9]', '''/* angle (0..ANGLES-1, counter clockwise, y up) of the vector (dx,dy) */
extern const u16 tan_q[46];             /* tan(a degrees) * 65536, a = 0..45 */
s16 AimAngle (s32 dy, s32 dx) B
{
\tu32 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
\tu16 ratio;
\ts16 a, lo, hi;
\tu8 swap = 0;

\tif (!ax && !ay)
\t\treturn 0;
\twhile (ax >= 32768 || ay >= 32768)
\t{
\t\tax >>= 1;
\t\tay >>= 1;
\t}
\tif (ay > ax)
\t{
\t\tu32 t = ax; ax = ay; ay = t;
\t\tswap = 1;
\t}
\tratio = (ay == ax) ? 65535 : udiv32_16(ay << 16, (u16)ax);
\tlo = 0; hi = 45;
\twhile (lo < hi)
\t{
\t\ts16 mid = (lo + hi) >> 1;
\t\tif (tan_q[mid] < ratio) lo = mid + 1; else hi = mid;
\t}
\ta = lo;
\tif (a > 0 && (u16)(ratio - tan_q[a - 1]) < (u16)(tan_q[a] - ratio))
\t\ta--;
\tif (swap) a = 90 - a;
\tif (dx < 0) a = 180 - a;
\tif (dy < 0) a = 360 - a;
\tif (a >= 360) a -= 360;
\treturn a;
}

''')
    # spawn: the ambush marker comes with the area map
    between('\tmap = &MAPSPOT0(tilex, tiley);\n\tif (*map == AMBUSHTILE)', '\tnewobj->obclass = guardobj+which;\n\tnewobj->hitpoints = starthitpoints[gamestate.difficulty][which];\n\tnewobj->dir = dir*2;', '''\tif (AreaOf(tilex, tiley) & 0x40)
\t\tnewobj->flags |= FL_AMBUSH;

''')
    between('void\tT_Gift (objtype *ob) B;', 'void T_Stand (objtype *ob) B', '')
    rep('\tu8 *map;\n\tu16 tile;\n\n\tswitch (which)\n\t{\n\tcase en_guard:\n\t\tSpawnNewObj (tilex,tiley,&s_grdstand);', '\tswitch (which)\n\t{\n\tcase en_guard:\n\t\tSpawnNewObj (tilex,tiley,&s_grdstand);')
    save()
    print('wl_act2.c patched')


if __name__ == '__main__':
    p_play()
    p_agent()
    p_act1()
    p_state()
    p_act2()
