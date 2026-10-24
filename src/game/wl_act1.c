#include "def.h"
/* wl_act1.c - statics, doors, pushable walls (port of WL_ACT1.C) */
#include <string.h>

statobj_t *laststatobj;

/* the info plane lives in ROM (InfoPlane); the few tiles the game clears at
 * run time (a pushed secret wall loses its marker) are remembered in a short
 * list */
#define MAXCLEARED 32
u8 info_clr_x[MAXCLEARED], info_clr_y[MAXCLEARED];
u8 info_clr_n;

/* the push walls change the tile map: a journal of the tiles they touched (latest value of each) lets a saved game
 * rebuild the level and replay them (save.c) */
#define PWJ_MAX 64
u8 pwj_n;
u8 pwj[PWJ_MAX][3];                     /* x | y << 6 | array << 12 (2 bytes), value */

static void pwj_set(u8 arr, s16 x, s16 y, u8 v)
{
    u8 i, lo = (u8)(x | ((y & 3) << 6)), hi = (u8)((y >> 2) | (arr << 4));
    for (i = 0; i < pwj_n; i++)
        if (pwj[i][0] == lo && pwj[i][1] == hi)
        {
            pwj[i][2] = v;
            return;
        }
    if (pwj_n < PWJ_MAX)
    {
        pwj[pwj_n][0] = lo;
        pwj[pwj_n][1] = hi;
        pwj[pwj_n][2] = v;
        pwj_n++;
    }
}

static void pw_tm(s16 x, s16 y, u8 v)
{
    tilemap[x][y] = v;
    pwj_set(0, x, y, v);
}

static void pw_at(s16 x, s16 y, u8 v)
{
    actorat[x][y] = v;
    pwj_set(1, x, y, v);
}

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

/*
 * DOORS
 */
#define DOORWIDTH   0x7800
#define OPENTICS    300

doorobj_t *lastdoorobj;
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
    pwj_n = 0;
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

void OpenDoor (s16 door) B
{
    if (doorobjlist[door].action == dr_open)
        doorobjlist[door].ticcount = 0;
    else
    {
        if (doorobjlist[door].action == dr_closed)
            doors_busy++;
        doorobjlist[door].action = dr_opening;
    }
}

static void CloseDoor(s16 door)
{
    s16 tilex, tiley, area;
    objtype *check;

    tilex = doorobjlist[door].tilex;
    tiley = doorobjlist[door].tiley;

    if (actorat[tilex][tiley])
        return;

    if (player->tilex == tilex && player->tiley == tiley)
        return;

    if (doorobjlist[door].vertical)
    {
        if (player->tiley == tiley)
        {
            if (((player->x + MINDIST) >> TILESHIFT) == tilex)
                return;
            if (((player->x - MINDIST) >> TILESHIFT) == tilex)
                return;
        }
        check = ACTORAT(tilex - 1, tiley);
        if (check && ((check->x + MINDIST) >> TILESHIFT) == tilex)
            return;
        check = ACTORAT(tilex + 1, tiley);
        if (check && ((check->x - MINDIST) >> TILESHIFT) == tilex)
            return;
    }
    else
    {
        if (player->tilex == tilex)
        {
            if (((player->y + MINDIST) >> TILESHIFT) == tiley)
                return;
            if (((player->y - MINDIST) >> TILESHIFT) == tiley)
                return;
        }
        check = ACTORAT(tilex, tiley - 1);
        if (check && ((check->y + MINDIST) >> TILESHIFT) == tiley)
            return;
        check = ACTORAT(tilex, tiley + 1);
        if (check && ((check->y - MINDIST) >> TILESHIFT) == tiley)
            return;
    }

    area = doorobjlist[door].area1;
    if (areabyplayer[area])
        PlaySoundLocTile(CLOSEDOORSND, doorobjlist[door].tilex, doorobjlist[door].tiley);

    doorobjlist[door].action = dr_closing;
    actorat[tilex][tiley] = door | 0x80;
}

void OperateDoor (s16 door) B
{
    s16 lock = doorobjlist[door].lock;

    if (lock >= dr_lock1 && lock <= dr_lock4)
    {
        if (!(gamestate.keys & (1 << (lock - dr_lock1))))
        {
            SD_PlaySound(NOWAYSND);
            return;
        }
    }

    switch (doorobjlist[door].action)
    {
    case dr_closed:
    case dr_closing:
        OpenDoor(door);
        break;
    case dr_open:
    case dr_opening:
        CloseDoor(door);
        break;
    }
}

static void DoorOpen(s16 door)
{
    if ((doorobjlist[door].ticcount += tics) >= OPENTICS)
        CloseDoor(door);
}

static void DoorOpening(s16 door)
{
    s32 position;

    position = doorposition[door];
    if (!position)
    {
        ConnectAreas();

        if (areabyplayer[doorobjlist[door].area1])
            PlaySoundLocTile(OPENDOORSND, doorobjlist[door].tilex, doorobjlist[door].tiley);
    }

    position += (s32)tics << 10;
    if (position >= 0xffff)
    {
        position = 0xffff;
        doorobjlist[door].ticcount = 0;
        doorobjlist[door].action = dr_open;
        actorat[doorobjlist[door].tilex][doorobjlist[door].tiley] = 0;
    }

    doorposition[door] = position;
    ((u8 *)CR_DOORPOS)[door] = position >> 8;
    render_cast_dirty = 1;
}

static void DoorClosing(s16 door)
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

void MoveDoors (void) B
{
    s16 door;

    if (gamestate.victoryflag || !doors_busy)
        return;

    for (door = 0; door < doornum; door++)
        switch (doorobjlist[door].action)
        {
        case dr_open:
            DoorOpen(door);
            break;
        case dr_opening:
            DoorOpening(door);
            break;
        case dr_closing:
            DoorClosing(door);
            break;
        }
}

/*
 * PUSHABLE WALLS
 */
u16 pwallstate;
u16 pwallpos;
u16 pwallx, pwally;
s16 pwalldir;

void PushWall (s16 checkx, s16 checky, s16 dir) B
{
    s16 oldtile;
    s16 nx = checkx, ny = checky;

    if (pwallstate)
        return;

    oldtile = tilemap[checkx][checky];
    if (!oldtile)
        return;

    switch (dir)
    {
    case di_north: ny--; break;
    case di_east:  nx++; break;
    case di_south: ny++; break;
    case di_west:  nx--; break;
    }
    if (actorat[nx][ny])
    {
        SD_PlaySound(NOWAYSND);
        return;
    }
    pw_at(nx, ny, oldtile & 0x3F);
    pw_tm(nx, ny, oldtile);

    gamestate.secretcount++;
    pwallx = checkx;
    pwally = checky;
    pwalldir = dir;
    pwallstate = 1;
    pwallpos = 0;
    pw_tm(pwallx, pwally, tilemap[pwallx][pwally] | 0xc0);
    render_cast_dirty = 1;
    ClearInfo(pwallx, pwally);          /* remove P tile info */
    SD_PlaySound(PUSHWALLSND);
}

void MovePWalls (void) B
{
    s16 oldblock, oldtile;
    s16 nx, ny;

    if (!pwallstate)
        return;

    oldblock = pwallstate / 128;

    pwallstate += tics;

    if (pwallstate / 128 != oldblock)
    {
        oldtile = tilemap[pwallx][pwally] & 63;

        pw_tm(pwallx, pwally, 0);
        pw_at(pwallx, pwally, 0);
        SetAreaOverride(pwallx, pwally, player->areanumber);

        if (pwallstate > 256)
        {
            pwallstate = 0;
            return;
        }
        else
        {
            switch (pwalldir)
            {
            case di_north: pwally--; nx = pwallx; ny = pwally - 1; break;
            case di_east:  pwallx++; nx = pwallx + 1; ny = pwally; break;
            case di_south: pwally++; nx = pwallx; ny = pwally + 1; break;
            default:       pwallx--; nx = pwallx - 1; ny = pwally; break;
            }
            if (actorat[nx][ny])
            {
                pwallstate = 0;
                return;
            }
            pw_at(nx, ny, oldtile);
            pw_tm(nx, ny, oldtile);
            pw_tm(pwallx, pwally, oldtile | 0xc0);
        }
    }

    pwallpos = (pwallstate / 2) & 63;
    cr_pwpos = (u8)pwallpos;
    render_cast_dirty = 1;
}
