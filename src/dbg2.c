/* dbg2.c - more self checks of the deterministic build (make DET=1), bank 9: asm routines against the C they
 * replaced.  dbg_checks2() runs once, from dbg_state_hash (dbg.c) at game_frames == 1. */
#include "game/def.h"
#include <string.h>

static u16 rng;
static u16 rnd16(void)
{
    rng = rng * 25173u + 13849u;
    return rng;
}
/* ------------------------------------------------------------------------------------------------------------
 * The sound player's asm (snd_a.s) against the C code it replaced (the reference copies below are that code).
 *  step: the stream of every song and every effect of the ROM, tick by tick, until it ends or 6000 ticks: the
 *        voice records must be equal after every tick.
 *  mix:  random voice / mixer states, in two passes with the same random numbers, the asm first and then the
 *        reference.  The final mixer state of each pass is hashed and compared here; the PSG writes of the two
 *        passes (between marker writes) are compared by tests/t_sndref.py from the smstest psglog.
 * dbg_sndchk: mismatches (0: all equal); dbg_sndn: how many comparisons were made. */
#include "snd.h"
#include <string.h>
void snd_step(Voice *v);
void snd_mix(void);
void snd_init(void) B;
void SD_StartMusic(u16 song) B;
void SD_MusicOff(void) B;
u8 SD_PlaySound(u16 sound) B;
void SD_StopSound(void) B;
extern u8 DigiOn, SoundOn, MusicOn;
u16 dbg_sndchk, dbg_sndn, dbg_sndh0, dbg_sndh1;

static void ref_silence(Voice *v)
{
    v->vol[0] = v->vol[1] = v->vol[2] = v->vol[3] = 15;
}

static void ref_step(Voice *v)
{
    u8 b, ch;
    u16 off;
    if (!v->on || --v->wait)
        return;
    MapCtrl = 0;
    MapSlot2 = v->bank;
    for (;;)
    {
        b = *v->p++;
        if (!b)
        {
            off = v->p[0] | (v->p[1] << 8);
            if (!off)
            {
                v->on = 0;
                ref_silence(v);
                return;
            }
            v->p = v->p - 1 - off;
            continue;
        }
        if (!(b & 0x80))
        {
            v->wait = b;
            return;
        }
        ch = (b >> 5) & 3;
        v->vol[ch] = b & 15;
        if (b & 0x10)
        {
            if (ch == 3)
                v->nctl = *v->p++;
            else
            {
                v->per[ch] = v->p[0] | (v->p[1] << 8);
                v->p += 2;
            }
        }
    }
}

static void ref_mix(void)
{
    const Voice *s;
    u8 ch, v;
    u16 p;
    for (ch = 0; ch < 4; ch++)
    {
        if (pcm_on && (ch == 1 || ch == 2))
            continue;
        s = (sfx.on && ch >= 2) ? &sfx : &mus;
        if (ch < 3)
        {
            p = s->per[ch];
            if (p != cur_per[ch])
            {
                cur_per[ch] = p;
                PSGPort = 0x80 | (ch << 5) | (p & 15);
                PSGPort = (u8)(p >> 4) & 0x3F;
            }
        }
        else if (s->nctl != cur_nctl)
        {
            cur_nctl = s->nctl;
            PSGPort = 0xE0 | (cur_nctl & 7);
        }
        v = s->vol[ch];
        if (v != cur_vol[ch])
        {
            cur_vol[ch] = v;
            PSGPort = 0x90 | (ch << 5) | v;
        }
    }
}

static void check_step(u8 i)
{
    Voice a, b;
    u16 t;
    if (i < NUM_SONGS)
    {
        SD_MusicOff();
        SD_StartMusic(i);
        a = mus;
        SD_MusicOff();
    }
    else
    {
        SD_StopSound();
        SD_PlaySound(i - NUM_SONGS);
        a = sfx;
        SD_StopSound();
    }
    if (!a.on)
        return;
    b = a;
    for (t = 0; t < 6000; t++)
    {
        snd_step(&a);
        ref_step(&b);
        dbg_sndn++;
        if (memcmp(&a, &b, sizeof(Voice)))
        {
            dbg_sndchk++;
            return;
        }
        if (!a.on)
            return;
    }
}

static u16 sr;
static u8 srnd(void)
{
    sr = sr * 25173u + 13849u;
    return (u8)(sr >> 8);
}

static u16 srnd16(void)
{
    u8 hi = srnd();
    return ((u16)hi << 8) | srnd();
}

/* a random mixer state: every comparison of mix() comes out equal or different about half of the time */
static void rnd_mixstate(void)
{
    u8 i;
    const Voice *s, *s2;
    sfx.on = srnd() & 1;
    pcm_on = srnd() % 3;
    for (i = 0; i < 3; i++)
    {
        mus.per[i] = srnd16();
        sfx.per[i] = srnd16();
    }
    for (i = 0; i < 4; i++)
    {
        mus.vol[i] = srnd() & 15;
        sfx.vol[i] = srnd() & 15;
    }
    mus.nctl = srnd();
    sfx.nctl = srnd();
    s2 = sfx.on ? &sfx : &mus;
    for (i = 0; i < 3; i++)
    {
        s = i >= 2 ? s2 : &mus;
        cur_per[i] = (srnd() & 1) ? s->per[i] : srnd16();
    }
    for (i = 0; i < 4; i++)
    {
        s = i >= 2 ? s2 : &mus;
        cur_vol[i] = (srnd() & 1) ? s->vol[i] : srnd();
    }
    cur_nctl = (srnd() & 1) ? s2->nctl : srnd();
}

static u16 mix_hash(void)
{
    u16 x = 0x5A5A;
    u8 i;
    for (i = 0; i < 3; i++)
        x = x * 31 + cur_per[i];
    for (i = 0; i < 4; i++)
        x = x * 31 + cur_vol[i];
    return x * 31 + cur_nctl;
}

static void psg_marker(void)
{
    u8 k;
    for (k = 0; k < 2; k++)
    {
        PSGPort = 0x9F; PSGPort = 0xBF; PSGPort = 0xDF; PSGPort = 0xFF;
    }
}

static void check_mix_pass(u8 ref)
{
    u16 n;
    sr = 0xBEEF;
    psg_marker();
    for (n = 0; n < 1500; n++)
    {
        rnd_mixstate();
        if (ref)
            ref_mix();
        else
            snd_mix();
        dbg_sndn++;
        if (ref)
            dbg_sndh1 = dbg_sndh1 * 7 + mix_hash();
        else
            dbg_sndh0 = dbg_sndh0 * 7 + mix_hash();
    }
    psg_marker();
}

static void check_snd(void)
{
    u8 i, ctrl = MapCtrl, bank2 = MapSlot2, dg = DigiOn;
    dbg_sndchk = dbg_sndn = dbg_sndh0 = dbg_sndh1 = 0;
    DigiOn = 0;                             /* the effects, not their digitised versions */
    for (i = 0; i < NUM_SONGS + NUM_SFX; i++)
        check_step(i);
    DigiOn = dg;
    check_mix_pass(0);
    check_mix_pass(1);
    if (dbg_sndh0 != dbg_sndh1)
        dbg_sndchk++;
    snd_init();
    MapSlot2 = bank2;
    MapCtrl = ctrl;
}

/* rd_buckets (sprites_a.s) against the C loop of render_draw it replaced, on random sprite lists and column
 * states (sprites off either edge, wider than the view, behind and in front of the walls).  dbg_bkchk: mismatches */
u16 dbg_bkchk, dbg_bkn;
extern u8 col_info[], col_ns[], col_lst[][MAXVISIBLE], spr_any;
extern u16 col_spr[], spr_new[];
void rd_buckets(void);
static u8 bk_info[VIEW_TW * 4], bk_ns[VIEW_TW], bk_any;
static u16 bk_lsth;
static u16 lst_hash(void)
{
    u16 h = 1, n;
    const u8 *p = &col_lst[0][0];
    for (n = 0; n < VIEW_TW * MAXVISIBLE; n++)
        h = ((h << 1) | (h >> 15)) + p[n];
    return h;
}
static u16 bk_spr[VIEW_TW], bk_new[VIEW_TW];

static void ref_buckets(void)
{
    u8 tc, i, *ci;
    VisSprite *v;
    memset(spr_new, 0, VIEW_TW * 2);
    memset(col_ns, 0, VIEW_TW);
    for (i = 0, v = vissprites; i < numvis; i++, v++)
    {
        s16 a0 = v->x0 >> 3, a1 = (v->x0 + (s16)v->width - 1) >> 3;
        u8 s = v->s, ya, yb;
        u16 sig;
        if (a0 < 0) a0 = 0;
        if (a1 > VIEW_TW - 1) a1 = VIEW_TW - 1;
        if (a0 > a1) continue;
        ya = s > VIEW_CY ? 0 : VIEW_CY - s;
        yb = s > VIEW_CY ? VIEW_H : VIEW_CY + s;
        sig = (u16)v->x0 + ((u16)s << 6) + ((u16)v->shape << 9) + v->width + 1;
        ci = col_info + ((u8)a0 << 2);
        for (tc = (u8)a0; tc <= (u8)a1; tc++, ci += 4)
        {
            if (s >= ci[3])
            {
                col_lst[tc][col_ns[tc]++] = i;
                spr_new[tc] += sig;
                if (ya < ci[1]) ci[1] = ya;
                if (yb > ci[2]) ci[2] = yb;
            }
        }
    }
    spr_any = numvis;
    for (tc = 0, ci = col_info; tc < VIEW_TW; tc++, ci += 4)
    {
        if (spr_new[tc] != col_spr[tc])
        {
            col_spr[tc] = spr_new[tc];
            ci[0] = 1;
        }
    }
}

static void check_buckets(void)
{
    u16 seq;
    u8 i, k, sv_num = numvis, sv_any = spr_any;
    static u8 sv_info[VIEW_TW * 4];
    static u16 sv_spr[VIEW_TW];
    static VisSprite sv_vis[MAXVISIBLE];
    memcpy(sv_info, col_info, sizeof(sv_info));
    memcpy(sv_spr, col_spr, sizeof(sv_spr));
    memcpy(sv_vis, vissprites, sizeof(sv_vis));
    dbg_bkchk = dbg_bkn = 0;
    for (seq = 0; seq < 120; seq++)
    {
        numvis = (u8)(rnd16() % (MAXVISIBLE + 1));
        for (i = 0; i < numvis; i++)
        {
            k = (u8)rnd16();
            vissprites[i].x0 = (k & 3) ? (s16)(rnd16() % 320) - 80 : (s16)rnd16();
            vissprites[i].width = (k & 12) ? 1 + rnd16() % 260 : rnd16();
            vissprites[i].s = (u8)rnd16();
            vissprites[i].shape = (u8)rnd16();
        }
        for (i = 0; i < VIEW_TW * 4; i++)
            col_info[i] = (i & 3) == 3 ? (u8)(rnd16() % 140) : (u8)rnd16();
        for (i = 0; i < VIEW_TW; i++)
        {
            col_spr[i] = (seq & 1) ? rnd16() : 0;
            spr_new[i] = rnd16();
            col_ns[i] = (u8)rnd16();
        }
        memset(col_lst, 0x5A, VIEW_TW * MAXVISIBLE);
        spr_any = 0x77;
        /* the starting state, then the asm, saved; the starting state again, the reference */
        memcpy(bk_info, col_info, sizeof(bk_info));
        memcpy(bk_spr, col_spr, sizeof(bk_spr));
        rd_buckets();
        {
            u8 t_info[VIEW_TW * 4];
            u16 t_spr[VIEW_TW];
            memcpy(t_info, col_info, sizeof(t_info));
            memcpy(t_spr, col_spr, sizeof(t_spr));
            memcpy(col_info, bk_info, sizeof(bk_info));
            memcpy(col_spr, bk_spr, sizeof(bk_spr));
            memcpy(bk_info, t_info, sizeof(t_info));
            memcpy(bk_spr, t_spr, sizeof(t_spr));
        }
        memcpy(bk_ns, col_ns, sizeof(bk_ns));
        bk_lsth = lst_hash();
        memcpy(bk_new, spr_new, sizeof(bk_new));
        bk_any = spr_any;
        memset(col_lst, 0x5A, VIEW_TW * MAXVISIBLE);
        spr_any = 0x77;
        ref_buckets();
        dbg_bkn++;
        if (memcmp(bk_info, col_info, sizeof(bk_info)) || memcmp(bk_spr, col_spr, sizeof(bk_spr))
            || memcmp(bk_ns, col_ns, sizeof(bk_ns)) || bk_lsth != lst_hash()
            || memcmp(bk_new, spr_new, sizeof(bk_new)) || bk_any != spr_any)
            dbg_bkchk++;
    }
    /* (col_ns, col_lst and spr_new are rebuilt before they are read) */
    memcpy(col_info, sv_info, sizeof(sv_info));
    memcpy(col_spr, sv_spr, sizeof(sv_spr));
    memcpy(vissprites, sv_vis, sizeof(sv_vis));
    numvis = sv_num;
    spr_any = sv_any;
}


/* the 16 x 16 -> 32 multiplies (mulfix.s) against shift and add (no multiply), on edge and random operands */
u16 dbg_umulchk, dbg_umuln;
static u32 ref_umul(u16 a, u16 b)
{
    u32 r = 0, m = b;
    u8 k;
    for (k = 0; k < 16; k++, m <<= 1, a >>= 1)
        if (a & 1)
            r += m;
    return r;
}

static void check_umul(void)
{
    static const u16 edge[] = { 0, 1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x101, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF, 0x1234, 0x12D2 };
    u16 n, a, b;
    u32 r;
    s32 sr;
    dbg_umulchk = dbg_umuln = 0;
    for (n = 0; n < 1225; n++)
    {
        if (n < 225)
        {
            a = edge[n / 15];
            b = edge[n % 15];
        }
        else
        {
            a = rnd16();
            b = rnd16();
            if (n & 1) a &= 0xFF;
            if (n & 2) b >>= (u8)(a & 7);
        }
        dbg_umuln++;
        r = (u32)a * b;
        if (r != ref_umul(a, b))
            dbg_umulchk++;
        /* signed: |a| * |b| with the sign */
        sr = (s32)(s16)a * (s16)b;
        r = ref_umul((s16)a < 0 ? -a : a, (s16)b < 0 ? -b : b);
        if (((s16)a < 0) != ((s16)b < 0))
            r = -r;
        if ((u32)sr != r)
            dbg_umulchk++;
        if ((u32)smul16((s16)a, (s16)b) != r)
            dbg_umulchk++;
    }
}


/* TryMove (agent_a.s, with the C for objects near the map's edge) against the original C, for the player put at
 * random places of the level: anywhere, and near the actors (dbg_tmchk: mismatches, dbg_tmn tests, dbg_tmno blocked) */
u16 dbg_tmchk, dbg_tmn, dbg_tmno;
boolean TryMove(objtype *ob) B;

static boolean ref_trymove(objtype *ob)
{
    s16 xl, yl, xh, yh, x, y;
    objtype *check;
    s32 deltax, deltay;

    xl = (ob->x - PLAYERSIZE) >> TILESHIFT;
    yl = (ob->y - PLAYERSIZE) >> TILESHIFT;
    xh = (ob->x + PLAYERSIZE) >> TILESHIFT;
    yh = (ob->y + PLAYERSIZE) >> TILESHIFT;
    for (y = yl; y <= yh; y++)
        for (x = xl; x <= xh; x++)
        {
            u8 a = actorat[x][y];
            if (a && !ACT_ISACTOR(a))
                return FALSE;
        }
    if (yl > 0) yl--;
    if (yh < MAPSIZE - 1) yh++;
    if (xl > 0) xl--;
    if (xh < MAPSIZE - 1) xh++;
    for (y = yl; y <= yh; y++)
        for (x = xl; x <= xh; x++)
        {
            check = ACTORAT(x, y);
            if (check && check != objlist && (check->flags & FL_SHOOTABLE))
            {
                deltax = ob->x - check->x;
                if (deltax < -MINACTORDIST || deltax > MINACTORDIST)
                    continue;
                deltay = ob->y - check->y;
                if (deltay < -MINACTORDIST || deltay > MINACTORDIST)
                    continue;
                return FALSE;
            }
        }
    return TRUE;
}

static void check_trymove(void)
{
    s32 sx = player->x, sy = player->y;
    u16 n;
    objtype *o;
    u8 k;
    dbg_tmchk = dbg_tmn = dbg_tmno = 0;
    for (n = 0; n < 1500; n++)
    {
        if (n & 1)
        {
            /* near an actor of the list */
            for (o = player->next, k = (u8)(rnd16() & 15); o && k; o = o->next, k--)
                ;
            if (!o)
                o = player;
            player->x = o->x + (s32)(s16)(rnd16() % 0x6000) - 0x3000 + (((s32)(s16)(rnd16() & 3) - 1) << 16);
            player->y = o->y + (s32)(s16)(rnd16() % 0x6000) - 0x3000 + (((s32)(s16)(rnd16() & 3) - 1) << 16);
        }
        else
        {
            player->x = ((s32)(rnd16() % MAPSIZE) << 16) | rnd16();
            player->y = ((s32)(rnd16() % MAPSIZE) << 16) | rnd16();
        }
        /* inside the map: the C's own limits (its loops read actorat[-1..64]) */
        if (player->x < 0x8000l || player->y < 0x8000l || player->x >= ((s32)MAPSIZE << 16) - 0x8000l
            || player->y >= ((s32)MAPSIZE << 16) - 0x8000l)
            continue;
        dbg_tmn++;
        k = TryMove(player);
        if (!k)
            dbg_tmno++;
        if (k != ref_trymove(player))
            dbg_tmchk++;
    }
    player->x = sx;
    player->y = sy;
}

/* tpath_fast (fast.s) against T_Path, on copies of the level's actors with random directions, distances, speeds,
 * reaction counts and step lengths: when it says done, T_Path must leave the same object; when it doesn't, the
 * object must be unchanged (dbg_tpchk: mismatches, dbg_tpn: tests, dbg_tpfast: done by tpath_fast) */
u16 dbg_tpchk, dbg_tpn, dbg_tpfast;
void T_Path(objtype *ob) B;
u8 tpath_fast(objtype *ob);

static void check_tpath(void)
{
    static objtype a, b, c;
    s16 st = tics;
    objtype *o;
    u16 n;
    u8 k;
    dbg_tpchk = dbg_tpn = dbg_tpfast = 0;
    for (n = 0; n < 2000; n++)
    {
        for (o = player->next, k = (u8)(rnd16() & 31); o && k; o = o->next, k--)
            ;
        if (!o)
            continue;
        a = *o;
        k = (u8)rnd16();
        a.dir = (k & 7) ? (u8)(rnd16() & 7) : (u8)(rnd16() % 10);
        a.distance = (k & 0x18) ? (s32)rnd16() + (s32)(rnd16() & 1) * 0x10000l : -(s32)(rnd16() & 3);
        if (k & 2)
            a.areanumber = (u8)(rnd16() % NUMAREAS);
        a.speed = (k & 0x20) ? (s32)(rnd16() % 2400) : (s32)rnd16() + ((s32)rnd16() << 16);
        a.temp2 = (k & 0x40) ? (s16)(rnd16() % 30) : 0;
        if (!(k & 0x80))
            a.flags ^= FL_ATTACKMODE;
        tics = (k & 1) ? 1 + rnd16() % 10 : (s16)(rnd16() % 31);
        a.flags &= ~FL_ATTACKMODE;      /* (SightPlayer quits on it; tpath_fast must leave it to T_Path) */
        if (!(k & 0x80))
            a.flags |= FL_ATTACKMODE;
        b = a;
        c = a;
        dbg_tpn++;
        if (tpath_fast(&a))
        {
            dbg_tpfast++;
            T_Path(&b);
            if (memcmp(&a, &b, sizeof(objtype)))
                dbg_tpchk++;
        }
        else if (memcmp(&a, &c, sizeof(objtype)))
            dbg_tpchk++;
    }
    tics = st;
}

void dbg_checks2(void) B
{
    rng = 12345;
    check_snd();
    check_buckets();
    check_umul();
    check_trymove();
    check_tpath();
}
