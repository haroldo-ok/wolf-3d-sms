/* save.c - three saved games and the score table in cartridge RAM page 1 (banked)
 *
 * The level is not stored, it is rebuilt (SetupGameLevel) and then only what the player changed is applied: the
 * game state, the doors, the push wall journal, the area overrides, the statics and the actors.  The stream is
 * run length coded for zeros and checked with a sum.  Cart RAM page 0 (the level) and page 1 (the slots) share
 * slot 2: everything goes through a small buffer. */
#include "ui.h"
#include <string.h>

#define CR1             0x8000
#define HDR_MAGIC       0x5753      /* 'WS' */
#define SLOT_MAGIC      0x5746
#define SLOT_BASE(k)    (0x0100 + (u16)(k) * 0x1500)
#define SLOT_SIZE       0x1500
#define SLOT_HDR        13          /* magic 2, floor, diff, lives, score 4, signature 2, length 2 (checksum follows) */
#define SIG             ((u16)SD_Startup)

extern u8 aovr[40][3], aovr_n;
extern u8 info_clr_x[], info_clr_y[], info_clr_n;
void *Scores_Ptr(void) B;
u16 Scores_Size(void) B;
void ConnectAreas(void) B;
void AreaCacheReset(void);
void InitActorList(void) B;
void GetNewActor(void) B;
void SetupGameLevel(void) B;
extern objtype *newobj;
extern objtype objlist[];

/* ------------------------------------------------------ cart RAM page 1 */
static void cr1_read(u16 off, u8 *dst, u16 n)
{
    u8 old = slot2_cur;
    slot2_map(SLOT2_CARTRAM1);
    memcpy(dst, (const u8 *)(CR1 + off), n);
    slot2_map(old);
}

static void cr1_write(u16 off, const u8 *src, u16 n)
{
    u8 old = slot2_cur;
    slot2_map(SLOT2_CARTRAM1);
    memcpy((u8 *)(CR1 + off), src, n);
    slot2_map(old);
}

static u16 sum_of(const u8 *p, u16 n, u16 s)
{
    while (n--)
        s = (s << 1 | s >> 15) ^ *p++;
    return s;
}

/* ---------------------------------------------------------- scores */
void Scores_Load(void) B
{
    u8 hdr[4];
    u8 tmp[160];
    u16 n = Scores_Size();
    cr1_read(0, hdr, 4);
    if ((hdr[0] | (hdr[1] << 8)) == HDR_MAGIC && n <= sizeof(tmp))
    {
        cr1_read(4, tmp, n);
        if (sum_of(tmp, n, 0) == (u16)(hdr[2] | (hdr[3] << 8)))
            memcpy(Scores_Ptr(), tmp, n);
    }
}

void Scores_Store(void) B
{
    u8 hdr[4];
    u16 n = Scores_Size(), s = sum_of(Scores_Ptr(), n, 0);
    hdr[0] = HDR_MAGIC & 0xFF;
    hdr[1] = HDR_MAGIC >> 8;
    hdr[2] = (u8)s;
    hdr[3] = (u8)(s >> 8);
    cr1_write(4, Scores_Ptr(), n);
    cr1_write(0, hdr, 4);
}

/* ------------------------------------------------------------- writer */
static u8 buf[96];
static u8 bn;                           /* bytes in buf */
static u16 pos, limit, sum;             /* where the next buffer goes, end of the slot, running sum */
static u8 zrun, bad;

static void flush(void)
{
    if (bn)
    {
        if (pos + bn > limit)
            bad = 1;
        else
            cr1_write(pos, buf, bn);
        pos += bn;
        bn = 0;
    }
}

static void wraw(u8 b)
{
    sum = (sum << 1 | sum >> 15) ^ b;
    buf[bn++] = b;
    if (bn == sizeof(buf))
        flush();
}

static void wzero(void)
{
    if (zrun)
    {
        wraw(0);
        wraw(zrun);
        zrun = 0;
    }
}

static void wb(u8 b)
{
    if (!b)
    {
        if (++zrun == 255)
            wzero();
        return;
    }
    wzero();
    wraw(b);
}

static void w16(u16 v) { wb((u8)v); wb((u8)(v >> 8)); }
static void w32(u32 v) { w16((u16)v); w16((u16)(v >> 16)); }

static void wmem(const void *p, u16 n)
{
    const u8 *s = p;
    while (n--)
        wb(*s++);
}

/* ------------------------------------------------------------- reader */
static u8 rbuf[96];
static u8 ri, rn;
static u16 rpos, rend, rzleft;
static u8 rzero;

static u8 rraw(void)
{
    if (ri == rn)
    {
        u8 n = (u8)((u16)(rend - rpos) > sizeof(rbuf) ? sizeof(rbuf) : rend - rpos);
        if (!n)
        {
            bad = 1;
            return 0;
        }
        cr1_read(rpos, rbuf, n);
        rpos += n;
        rn = n;
        ri = 0;
    }
    return rbuf[ri++];
}

static u8 rb(void)
{
    u8 b;
    if (rzleft)
    {
        rzleft--;
        return 0;
    }
    b = rraw();
    if (b)
        return b;
    rzleft = rraw();
    if (!rzleft)
        return 0;
    rzleft--;
    return 0;
}

static u16 r16(void) { u16 l = rb(); return l | ((u16)rb() << 8); }
static u32 r32(void) { u32 l = r16(); return l | ((u32)r16() << 16); }

static void rmem(void *p, u16 n)
{
    u8 *d = p;
    while (n--)
        *d++ = rb();
}

/* -------------------------------------------------------------- slots */
u8 Save_Info(u8 slot, SaveInfo *si) B
{
    u8 h[SLOT_HDR];
    cr1_read(SLOT_BASE(slot), h, SLOT_HDR);
    if ((h[0] | (h[1] << 8)) != SLOT_MAGIC)
        return 0;
    si->floor = h[2];
    si->diff = h[3];
    si->lives = h[4];
    si->score = h[5] | ((u32)h[6] << 8) | ((u32)h[7] << 16) | ((u32)h[8] << 24);
    return 1;
}

u16 save_len;                           /* size of the last stream (the test reports it) */

u8 Save_Write(u8 slot) B
{
    u8 h[SLOT_HDR + 2], i;
    u16 base = SLOT_BASE(slot), n, len, w;
    objtype *o;
    const statobj_t *st;

    memset(h, 0, sizeof(h));
    cr1_write(base, h, 2);                      /* not valid while it is written */
    pos = base + SLOT_HDR + 2;
    limit = base + SLOT_SIZE;
    sum = 0;
    bn = zrun = bad = 0;

    wmem(&gamestate, sizeof(gamestate));
    wmem(ratio_kill, 8); wmem(ratio_secret, 8); wmem(ratio_treasure, 8);
    wmem(ratio_time, 16);
    w16(pwallstate); w16(pwallpos); w16(pwallx); w16(pwally); w16((u16)pwalldir);

    wb(pwj_n);
    wmem(pwj, pwj_n * 3);
    wb(aovr_n);
    wmem(aovr, aovr_n * 3);
    wb(info_clr_n);
    wmem(info_clr_x, info_clr_n);
    wmem(info_clr_y, info_clr_n);

    wb((u8)doornum);
    for (i = 0; i < doornum; i++)
    {
        wmem(&doorobjlist[i], sizeof(doorobj_t));
        w16(doorposition[i]);
    }

    n = (u16)(laststatobj - statobjlist);
    w16(n);
    for (st = statobjlist; n--; st++)
    {
        wb(st->tilex);
        wb(st->tiley);
        wb(st->type == 0 ? 0xFE : st->type);    /* 0 is a valid type: zero bytes are run length coded, keep it */
    }

    n = 0;
    for (o = player; o; o = o->next)
        n++;
    w16(n);
    for (o = player; o; o = o->next)
    {
        wb((u8)o->active); w16(o->ticcount); wb(o->obclass); w16((u16)o->state); wb(o->flags);
        w32(o->distance); wb(o->dir); w32(o->x); w32(o->y); wb(o->areanumber); w16(o->angle);
        w16(o->hitpoints); w32(o->speed); w16(o->temp1); w16(o->temp2); w16(o->temp3);
        wb(o->tilex); wb(o->tiley);             /* not x >> 16: a walker claims the tile it is heading for */
        wb(actorat[o->tilex][o->tiley] == o->enc ? 1 : 2);
    }
    wzero();
    flush();
    if (bad)
        return 0;
    len = pos - (base + SLOT_HDR + 2);
    save_len = len;
    w = SLOT_MAGIC;
    h[0] = (u8)w; h[1] = w >> 8;
    h[2] = (u8)(gamestate.mapon + 1);
    h[3] = (u8)gamestate.difficulty;
    h[4] = (u8)(gamestate.lives > 0 ? gamestate.lives : 0);
    h[5] = (u8)gamestate.score; h[6] = (u8)(gamestate.score >> 8);
    h[7] = (u8)(gamestate.score >> 16); h[8] = (u8)(gamestate.score >> 24);
    w = SIG;
    h[9] = (u8)w; h[10] = w >> 8;
    h[11] = (u8)len; h[12] = len >> 8;
    h[13] = (u8)sum; h[14] = (u8)(sum >> 8);
    cr1_write(base, h, SLOT_HDR + 2);
    return 1;
}

u8 Save_Read(u8 slot) B
{
    u8 h[SLOT_HDR + 2], i, reg;
    u16 base = SLOT_BASE(slot), n, len, want, k;
    u8 x, y;
    gametype gs;
    u8 rk[8], rs[8], rt[8];
    u16 rtm[8], pw[5];
    objtype *o;
    objtype tmp;

    cr1_read(base, h, SLOT_HDR + 2);
    if ((h[0] | (h[1] << 8)) != SLOT_MAGIC || (u16)(h[9] | (h[10] << 8)) != SIG)
        return 0;
    len = h[11] | (h[12] << 8);
    want = h[13] | (h[14] << 8);
    if (len > SLOT_SIZE - SLOT_HDR - 2)
        return 0;
    /* checksum of the stream as stored */
    rpos = base + SLOT_HDR + 2;
    rend = rpos + len;
    ri = rn = 0;
    bad = 0;
    sum = 0;
    for (n = 0; n < len; n++)
        sum = (sum << 1 | sum >> 15) ^ rraw();
    if (bad || sum != want)
        return 0;
    rpos = base + SLOT_HDR + 2;
    ri = rn = 0;
    rzleft = 0;
    bad = 0;

    rmem(&gs, sizeof(gs));
    if (gs.mapon < 0 || gs.mapon > 9)
        return 0;
    slot2_map(SLOT2_CARTRAM);
    gamestate = gs;
    SetupGameLevel();                           /* the level as it was at the start */
    gamestate = gs;
    rmem(rk, 8); rmem(rs, 8); rmem(rt, 8); rmem(rtm, 16);
    memcpy(ratio_kill, rk, 8); memcpy(ratio_secret, rs, 8); memcpy(ratio_treasure, rt, 8);
    memcpy(ratio_time, rtm, 16);
    for (i = 0; i < 5; i++)
        pw[i] = r16();
    pwallstate = pw[0]; pwallpos = pw[1]; pwallx = pw[2]; pwally = pw[3]; pwalldir = (s16)pw[4];
    cr_pwpos = (u8)pwallpos;

    pwj_n = rb();
    if (pwj_n > 64) pwj_n = 0;
    rmem(pwj, pwj_n * 3);
    for (i = 0; i < pwj_n; i++)                 /* replay the push walls */
    {
        x = pwj[i][0] & 63;
        y = (u8)((pwj[i][0] >> 6) | ((pwj[i][1] & 15) << 2));
        if (pwj[i][1] & 0x10)
            actorat[x][y] = pwj[i][2];
        else
            tilemap[x][y] = pwj[i][2];
    }
    aovr_n = rb();
    if (aovr_n > 40) aovr_n = 0;
    rmem(aovr, aovr_n * 3);
    info_clr_n = rb();
    if (info_clr_n > 32) info_clr_n = 0;
    rmem(info_clr_x, info_clr_n);
    rmem(info_clr_y, info_clr_n);
    AreaCacheReset();

    /* the actors of the pristine level leave the actor map: the saved ones come back below */
    for (x = 0; x < 64; x++)
        for (y = 0; y < 64; y++)
            if (actorat[x][y] & 0x40)
                actorat[x][y] = 0;

    k = rb();
    if (k != doornum)
        bad = 1;
    doors_busy = 0;
    for (i = 0; i < k && i < doornum; i++)
    {
        rmem(&doorobjlist[i], sizeof(doorobj_t));
        doorposition[i] = r16();
        ((u8 *)CR_DOORPOS)[i] = (u8)(doorposition[i] >> 8);
        if (doorobjlist[i].action != dr_closed)
            doors_busy++;
        actorat[doorobjlist[i].tilex][doorobjlist[i].tiley] = doorobjlist[i].action == dr_open ? 0 : (u8)(i | 0x80);
    }

    n = r16();
    if (n > MAXSTATS)
        n = 0, bad = 1;
    for (k = 0; k < n; k++)
    {
        statobjlist[k].tilex = rb();
        statobjlist[k].tiley = rb();
        i = rb();
        statobjlist[k].type = i == 0xFE ? 0 : i;
    }
    laststatobj = &statobjlist[n];

    n = r16();
    InitActorList();
    for (k = 0; k < n && !bad; k++)
    {
        memset(&tmp, 0, sizeof(tmp));
        tmp.active = (s8)rb(); tmp.ticcount = (s16)r16(); tmp.obclass = rb(); tmp.state = (const statetype *)r16();
        tmp.flags = rb(); tmp.distance = (s32)r32(); tmp.dir = rb(); tmp.x = (s32)r32(); tmp.y = (s32)r32();
        tmp.areanumber = rb(); tmp.angle = (s16)r16(); tmp.hitpoints = (s16)r16(); tmp.speed = (s32)r32();
        tmp.temp1 = (s16)r16(); tmp.temp2 = (s16)r16(); tmp.temp3 = (s16)r16();
        tmp.tilex = rb();
        tmp.tiley = rb();
        reg = rb();
        if (k == 0)
            o = player;
        else
        {
            GetNewActor();
            o = newobj;
        }
        {
            struct objstruct *nx = o->next, *pv = o->prev;
            u8 enc = o->enc;
            *o = tmp;
            o->next = nx;
            o->prev = pv;
            o->enc = enc;
        }
        if (reg == 1)
            actorat[o->tilex][o->tiley] = o->enc;
    }
    if (bad)
        return 0;
    ConnectAreas();
    render_cast_dirty = 1;
    render_invalidate();
    STATICS_CHANGED();
    return 1;
}
