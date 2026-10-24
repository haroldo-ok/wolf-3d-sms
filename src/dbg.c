/* dbg.c - make DET=1 only (banked): dbg_hash, a hash of the whole game state, computed once per frame.
 * The equivalence tests (tests/det) compare it between builds: with a fixed time step and a constant
 * random seed the state after N frames is a pure function of the input script, so an optimisation
 * that must not change the game has to leave dbg_hash unchanged at every checkpoint.
 * Pointers are never hashed (their values move with the code): states are hashed as their index. */
#include "game/def.h"

#ifdef DEBUG_DET
extern u16 game_frames;
u16 dbg_hash;
static u16 h;

static void hb(u8 b)
{
    h = ((h << 5) | (h >> 11)) ^ b;
    h += 0x9E37;
}

static void hw(u16 w)
{
    hb((u8)w);
    hb((u8)(w >> 8));
}

static void hl(s32 v)
{
    hw((u16)v);
    hw((u16)(v >> 16));
}

/* the actorat codes: o->enc and act_dec() against their definition, for every actor slot (0: all correct) */
u16 dbg_actchk;
static void check_act(void)
{
    u8 id, e;
    const objtype *o;
    dbg_actchk = 0;
    for (id = 0; id < MAXACTORS; id++)
    {
        o = &objlist[id];
        e = 0x40 | (id & 0x3F) | ((id & 0x40) << 1);
        if (o->enc != e)
            dbg_actchk++;
        if (act_dec(e) != o)
            dbg_actchk += 0x100;
    }
    for (e = 0; e < 0x40; e++)
        if (act_dec(e) != 0 || act_dec(e | 0x80) != 0)
            dbg_actchk += 0x1000;
}

/* ---- the sprite projection (sprites_a.s) against the C code it replaced */
#include <string.h>
extern s16 xf_vx;
extern u8 xf_sh;
extern s32 xf_depth;
extern u8 vtx, vty, vang8;
extern void xf_frame(void);
extern u8 xf_obj(const objtype *ob);
extern u8 xf_tile(u8 tx, u8 ty);
extern void add_sprite(u8 sh);
extern void dbg_check_inview(void);
extern u16 dbg_ivchk;

#define KV_HALF_256 ((s32)PROJ_KV_HALF_256)
#define KH          ((s32)PROJ_KH)

static boolean ref_xform(fixed ox, fixed oy, s16 *vx, u8 *vs, s32 *depth)
{
    fixed gx, gy, nx, ny;
    s16 gx8, gy8, nx8, c14, s14, q;
    u16 sh;
    u32 num;

    gx = ox - viewx;
    gy = oy - viewy;
    if (gx >= 0x7F0000L || gx <= -0x7F0000L || gy >= 0x7F0000L || gy <= -0x7F0000L)
        return FALSE;
    gx8 = (s16)(gx >> 8);
    gy8 = (s16)(gy >> 8);
    c14 = (s16)(viewcos >> 2);
    s14 = (s16)(viewsin >> 2);
    nx = smul16(gx8, c14) - smul16(gy8, s14) - (0x4000L << 6);
    if (nx < (0x5800L << 6))
        return FALSE;
    if (nx >= (KV_HALF_256 << 14))
        return FALSE;
    nx8 = (s16)(nx >> 14);
    ny = smul16(gx8, s14) + smul16(gy8, c14);
    q = (s16)(ny >> 14);
    sh = (u16)KV_HALF_256 / (u16)nx8;
    if (sh > MAXS)
        sh = MAXS;
    num = (u32)(u16)(q < 0 ? -q : q) * (u32)KH;
    if ((u16)(num >> 9) >= (u16)nx8)
        return FALSE;
    if (q < 0)
        *vx = (VIEW_W / 2) - (s16)udiv32_16(num, nx8);
    else
        *vx = (VIEW_W / 2) + (s16)udiv32_16(num, nx8);
    *vs = (u8)sh;
    *depth = nx >> 6;
    return TRUE;
}

static u16 rng = 12345;
static u16 rnd16(void)
{
    rng = rng * 25173u + 13849u;
    return rng;
}

u16 dbg_xfchk, dbg_xfn, dbg_xfvis, dbg_xfneg;
static objtype fake;
static void check_xform(void)
{
    static const s32 vxs[6] = { 0x1D8000L, 0x1D8123L, 0x39C000L, 0x0A7FFFL, 0x2F0001L, 0x3F8000L };
    static const s32 vys[6] = { 0x39C000L, 0x39C456L, 0x1D8000L, 0x38A000L, 0x0C0100L, 0x2A7FFFL };
    static const s16 angs[6] = { 0, 123, 270, 359, 45, 181 };
    u8 i, j;
    s8 dx, dy;
    s16 rvx;
    u8 rvs;
    s32 rdepth;
    u8 r, a;

    dbg_xfchk = dbg_xfn = dbg_xfvis = dbg_xfneg = 0;
    for (i = 0; i < 6; i++)
    {
        viewx = vxs[i];
        viewy = vys[i];
        viewangle = angs[i];
        viewsin = fsin(viewangle);
        viewcos = fcos(viewangle);
        vtx = (u8)(viewx >> 16);
        vty = (u8)(viewy >> 16);
        xf_frame();
        for (dy = -14; dy <= 14; dy++)
            for (dx = -14; dx <= 14; dx++)
            {
                if ((u8)(vtx + dx) > 63 || (u8)(vty + dy) > 63)
                    continue;
                r = ref_xform(((s32)(u8)(vtx + dx) << 16) + 0x8000L, ((s32)(u8)(vty + dy) << 16) + 0x8000L, &rvx, &rvs, &rdepth);
                a = xf_tile(vtx + dx, vty + dy);
                dbg_xfn++;
                if (r)
                {
                    dbg_xfvis++;
                    if (rvx < VIEW_W / 2)
                        dbg_xfneg++;
                }
                if (r != a || (r && (rvx != xf_vx || rvs != xf_sh)))
                    dbg_xfchk++;
            }
        for (j = 0; j < 200; j++)
        {
            s32 ox = viewx + ((s32)(rnd16() & 0x3FFF) - 0x1FFF) * 120L;
            s32 oy = viewy + ((s32)(rnd16() & 0x3FFF) - 0x1FFF) * 120L;
            if (ox < 0x10000L) ox = 0x10000L;
            if (oy < 0x10000L) oy = 0x10000L;
            fake.x = ox;
            fake.y = oy;
            r = ref_xform(ox, oy, &rvx, &rvs, &rdepth);
            a = xf_obj(&fake);
            dbg_xfn++;
            if (r)
            {
                dbg_xfvis++;
                if (rvx < VIEW_W / 2)
                    dbg_xfneg++;
            }
            if (r != a || (r && (rvx != xf_vx || rvs != xf_sh || rdepth != xf_depth)))
                dbg_xfchk++;
        }
    }
}

/* add_sprite against the C list insertion, random sprites (some off the view, some with no shape), also
 * past MAXVISIBLE */
u16 dbg_addchk, dbg_addn, dbg_addfull;
static VisSprite ref_vis[MAXVISIBLE];
static u8 ref_num;
static void ref_add_sprite(u8 sh, s16 vx, u8 sc)
{
    u16 w;
    s16 x0;
    u8 j;
    VisSprite *v;

    if (!shape_width[sh])
        return;
    w = ((u16)shape_width[sh] * sc * PROJ_HSCALE_NUM / PROJ_HSCALE_DEN) >> 5;
    if (!w)
        return;
    x0 = vx - (s16)(w >> 1);
    if (x0 >= VIEW_W || x0 + (s16)w <= 0)
        return;
    if (ref_num == MAXVISIBLE)
    {
        if (sc <= ref_vis[0].s)
            return;
        memmove(&ref_vis[0], &ref_vis[1], (MAXVISIBLE - 1) * sizeof(VisSprite));
        ref_num--;
    }
    for (j = ref_num; j && ref_vis[j - 1].s > sc; j--)
        ref_vis[j] = ref_vis[j - 1];
    v = &ref_vis[j];
    v->x0 = x0;
    v->width = w;
    v->s = sc;
    v->shape = sh;
    ref_num++;
}

static void check_addsprite(void)
{
    u16 seq, n;
    u8 sh, sc;
    s16 vx;

    dbg_addchk = dbg_addn = dbg_addfull = 0;
    for (seq = 0; seq < 40; seq++)
    {
        numvis = 0;
        ref_num = 0;
        for (n = 0; n < 130; n++)
        {
            sh = (u8)(rnd16() % 435);
            sc = (u8)(1 + rnd16() % MAXS);
            vx = (s16)(rnd16() % 300) - 40;
            xf_vx = vx;
            xf_sh = sc;
            add_sprite(sh);
            ref_add_sprite(sh, vx, sc);
            dbg_addn++;
            if (ref_num == MAXVISIBLE)
                dbg_addfull++;
            if (numvis != ref_num || memcmp(vissprites, ref_vis, ref_num * sizeof(VisSprite)))
            {
                dbg_addchk++;
                break;
            }
        }
    }
    numvis = 0;
}


/* mul_tics and LineStep against the expressions they replace (0: all correct) */
u16 dbg_mulchk, dbg_stepchk;
s16 LineStep(s16 delta, s16 deltafrac) B;
static void check_math(void)
{
    u8 t;
    u16 v;
    s16 dl, df, r;
    s32 q;

    dbg_mulchk = dbg_stepchk = 0;
    for (t = 0; t < 250; t += 7)
        for (v = 0; v < 60000u; v += 1237)
            if (mul_tics(t, v) != (s32)t * v)
                dbg_mulchk++;
    for (dl = -9000; dl <= 9000; dl += 229)
        for (df = 1; df < 16000; df += 331)
        {
            q = ((s32)dl << 8) / df;
            r = q > 0x7fffl ? 0x7fff : (q < -0x7fffl ? -0x7fff : (s16)q);
            if (LineStep(dl, df) != r)
                dbg_stepchk++;
        }
}


void dbg_checks2(void) B;
u8 dbg_full, dbg_sv_diff;
u16 dbg_sec[8], dbg_sec0[8];
u16 dbg_f0[24], dbg_f1[24];
u8 dbg_sv_obj, dbg_sv_fld;
u16 dbg_sv_v0, dbg_sv_v1;
static u16 oh[40][2];

static void ofields(objtype *o, u16 *f)
{
    f[0] = o->obclass; f[1] = o->flags; f[2] = o->tilex; f[3] = o->tiley; f[4] = o->dir; f[5] = (u8)o->active;
    f[6] = o->areanumber; f[7] = (u16)o->x; f[8] = (u16)(o->x >> 16); f[9] = (u16)o->y; f[10] = (u16)(o->y >> 16);
    f[11] = (u16)o->distance; f[12] = (u16)(o->distance >> 16); f[13] = (u16)o->speed; f[14] = (u16)(o->speed >> 16);
    f[15] = o->hitpoints; f[16] = o->ticcount; f[17] = o->angle; f[18] = o->temp1; f[19] = o->temp2; f[20] = o->temp3;
    f[21] = (u16)(o->state - states);
}

static u16 ofsum(objtype *o)
{
    u16 f[24], i, s = 0;
    ofields(o, f);
    for (i = 0; i < 22; i++)
        s = (s << 1 | s >> 15) ^ f[i];
    return s;
}
u16 dbg_sv_a, dbg_sv_b;
u8 dbg_sv_w, dbg_sv_r, dbg_sv_ok;
u8 Save_Write(u8 slot) B;
u8 Save_Read(u8 slot) B;
void dbg_state_hash(void) B;

/* save, damage the state, load: the hash of the whole game must be the same again (maps included) */
void dbg_save_test(void) B
{
    objtype *o;
    u8 i;
    dbg_full = 1;
    dbg_state_hash();
    dbg_sv_a = dbg_hash;
    for (i = 0; i < 8; i++)
        dbg_sec0[i] = dbg_sec[i];
    {
        u8 k = 0;
        for (o = player; o && k < 40; o = o->next, k++)
        {
            oh[k][0] = ofsum(o);
            if (k == 11)
                ofields(o, dbg_f0);
        }
    }
    dbg_sv_w = Save_Write(0);
    gamestate.score += 777;
    gamestate.health = 3;
    gamestate.killcount += 5;
    player->x += 0x10000L;
    if (doornum)
        doorobjlist[0].action = dr_open;
    doors_busy = 3;
    pwallstate = 77;
    for (o = player->next; o; o = o->next)
    {
        o->hitpoints += 3;
        o->tilex ^= 1;
    }
    if (laststatobj != statobjlist)
        laststatobj--;
    tilemap[3][3] ^= 0x10;
    dbg_sv_r = Save_Read(0);
    loadedgame = FALSE;
    dbg_state_hash();
    dbg_sv_b = dbg_hash;
    dbg_sv_ok = dbg_sv_a == dbg_sv_b;
    {
        u8 k = 0;
        dbg_sv_obj = 0;
        dbg_sv_fld = 0;
        for (o = player; o && k < 40; o = o->next, k++)
            if (!dbg_sv_obj && ofsum(o) != oh[k][0])
            {
                dbg_sv_obj = k + 1;
                ofields(o, dbg_f1);
                for (i = 0; i < 22; i++)
                    if (!dbg_sv_fld && dbg_f0[i] != dbg_f1[i])
                    {
                        dbg_sv_fld = i + 1;
                        dbg_sv_v0 = dbg_f0[i];
                        dbg_sv_v1 = dbg_f1[i];
                    }
            }
    }
    dbg_sv_diff = 0;
    for (i = 0; i < 7; i++)
        if (!dbg_sv_diff && dbg_sec[i] != dbg_sec0[i])
            dbg_sv_diff = i + 1;
    dbg_full = 0;
}

void dbg_state_hash(void) B
{
    objtype *o;
    statobj_t *st;
    doorobj_t *d;
    const u8 *p;
    u16 i;

    check_act();
    if (game_frames == 1)
    {
        check_math();
        check_xform();
        check_addsprite();
        dbg_check_inview();
        dbg_checks2();          /* dbg2.c (bank 9) */
    }
    h = 0x1234;
    p = (const u8 *)&gamestate;
    for (i = 0; i < sizeof(gamestate); i++)
        hb(p[i]);
    dbg_sec[0] = h;
    for (o = player; o; o = o->next)
    {
        hb(o->obclass); hb(o->flags); hb(o->tilex); hb(o->tiley); hb(o->dir);
        hb(o->active); hb(o->areanumber);
        hl(o->x); hl(o->y); hl(o->distance); hl(o->speed);
        hw(o->hitpoints); hw(o->ticcount); hw(o->angle);
        hw(o->temp1); hw(o->temp2); hw(o->temp3);
        hw((u16)(o->state - states));
    }
    dbg_sec[1] = h;
    for (st = statobjlist; st != laststatobj; st++)
    {
        hb(st->tilex); hb(st->tiley); hb(st->type);
    }
    dbg_sec[2] = h;
    for (d = doorobjlist, i = 0; d != lastdoorobj; d++, i++)
    {
        hb(d->tilex); hb(d->tiley); hb(d->action); hb(d->lock); hw(d->ticcount);
        hw(doorposition[i]);
    }
    dbg_sec[3] = h;
    hw(pwallpos); hw(pwallstate); hb((u8)pwalldir);
    dbg_sec[4] = h;
    if (!(game_frames & 15) || dbg_full)    /* the 8 KB maps: every 16th frame only (slow) */
    {
        p = &tilemap[0][0];
        for (i = 0; i < MAPSIZE * MAPSIZE; i++)
            hb(p[i]);
        dbg_sec[5] = h;
        p = &actorat[0][0];
        for (i = 0; i < MAPSIZE * MAPSIZE; i++)
            hb(p[i]);
        dbg_sec[6] = h;
    }
    dbg_hash = h;
}
#endif
