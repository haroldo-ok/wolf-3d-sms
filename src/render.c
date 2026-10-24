/* render.c - SMS 3-D view: ray casting, wall/sprite column composition,
 * and bitplane conversion straight into the view's VRAM tiles.
 *
 * The view is 28x10 tiles; tile (tc, r) = tc*10 + r, so one tile column is
 * 320 contiguous VRAM bytes.  Each tile column holds two 4 pixel rays.
 * Cells that are entirely sky or ground point at two shared flat tiles
 * instead, so only the rows that hold walls, sprites or the hand are drawn.
 */
#include "game/def.h"
#include <string.h>
#include <stddef.h>

#define RD8(a)  ((const u8 *)(a))
#define RD16(a) ((const u16 *)(a))

#define COLBUF ((u8 *)0xDA00)

u16 r_viewx, r_viewy, r_fine;
u8  r_animframe;
u8  r_skycolor, r_groundcolor;
/* ray_s[] and ray_tbank[] live in render_a.s (adjacent, for rd_sig) */
VisSprite vissprites[MAXVISIBLE];
u8  numvis;

u8  ray_wall[NRAYS];     /* wall texture index, 0xFF: none */
u8  ray_u[NRAYS];
u16 ray_hit[NRAYS];
u8  ray_side[NRAYS];
extern u8 ray_tbank[];          /* texture bank / column address */
u16 ray_tex[NRAYS];

u8  scaler_bank0[MAXS + 1];
u16 scaler_addr0[MAXS + 1];
u8  sscaler_bank[MAXS + 1], sscaler_hi[MAXS + 1];
u8  spr_jp[3];                  /* jp nnnn trampoline (RAM) */
const u8 *spr_seg;
u8  spr_s;
void spr_ray(void);
void hand_rows(void);
u8  nt_state[VIEW_TW * VIEW_TH];
u8  nt_tc, nt_tr0, nt_tr1;
u8  nt_hold;             /* nonzero: don't write the nametable (fizzle) */
void nt_update(void);
u8  col_tr0[VIEW_TW], col_tr1[VIEW_TW];
u8  rd_tc;
void rd_cols(void);
u8  dr_c, dr_y0, dr_y1, dr_skyv, dr_gndv, colright;
void draw_ray(void);

/* asm interface */
u8  s1_rdbank;
u16 s2_vaddr, s2_hand;
u8  s2_y, s2_n;
u8   dda(void);
u16  mulfrac8(u16 d, u16 f);
u16  mq_a, mq_b;
u16  cr_p, cr_sx, cr_sX, cr_sY, cr_dX, cr_dY, cr_lim, cr_dist;
u8   cr_side, cr_ydir;
void s2_setaddr(void);
void s2_rows(void);

extern const u8 *light_tab, *dark_tab;

/* ------------------------------------------------------------ setup */

static void flat_tile(u16 tile, u8 color)
{
    u8 buf[32];
    u8 i;
    for (i = 0; i < 32; i++)
        buf[i] = (color >> (i & 3)) & 1 ? 0xFF : 0x00;
    vram_write_safe(tile * 32, buf, 32);
}

void render_set_colors(u8 sky, u8 ground)
{
    if (sky != r_skycolor)
    {
        r_skycolor = sky;
        flat_tile(TILE_SKY, sky);
    }
    if (ground != r_groundcolor)
    {
        r_groundcolor = ground;
        flat_tile(TILE_GROUND, ground);
    }
}

/* per tile column signature of everything that decides its pixels: when
 * it matches the previous frame the column is left alone.  col_sig holds
 * {s, tbank, tex lo, tex hi} per ray; rd_sig() compares and updates it and
 * fills col_info[tc] = {changed, ymin, ymax, min s} */
u8  col_sig[NRAYS * 4];
u8  col_info[VIEW_TW * 4];
void rd_sig(void);
void rd_ext(u8 chg);
u16 col_spr[VIEW_TW], spr_new[VIEW_TW];   /* (rd_buckets in sprites_a.s) */
u8  spr_any;
void rd_buckets(void);
static u8  sig_colors = 0xFF;
static u8  sig_reset = 1;           /* col_sig doesn't describe the screen: forget it (every column is drawn) */
extern u8 render_cast_dirty;

void render_invalidate(void)
{
    memset(nt_state, 0xFF, sizeof(nt_state));
    memset(col_tr0, 0xFF, sizeof(col_tr0));
    sig_colors = 0xFF;
    sig_reset = 1;
    render_cast_dirty = 1;
}

void render_init(void)
{
    u8 old = MapSlot1;
    u8 i;
    MapSlot1 = RD_BANK;
    for (i = 0; i <= MAXS; i++)
    {
        scaler_bank0[i] = RD8(RD_SCALER_BANK)[i];
        scaler_addr0[i] = RD16(RD_SCALER_ADDR)[i];
        sscaler_bank[i] = RD8(RD_SSCALER_BANK)[i];
        sscaler_hi[i] = RD8(RD_SSCALER_HI)[i];
    }
    MapSlot1 = old;
    spr_jp[0] = 0xC3;
    light_tab = RD8(RD_WALLX);
    dark_tab = RD8(RD_WALLY);
    s1_rdbank = RD_BANK;
    r_skycolor = 0xFF;
    r_groundcolor = 0xFF;
    render_set_colors(0, 0);
    flat_tile(TILE_BLANK, 0);
    render_invalidate();
}

/* ------------------------------------------------------------ casting */

u8  fxn, fyn;            /* camera position inside its tile (x/256) */
u8 *cast_p0;
const u8 *light_tab, *dark_tab;

u8 cr_c;
void cast_ray(void);
void set_tex_asm(void);

void cast_all(void);
static u8 cast_mode;                /* since the last draw: 0 rays unchanged, 1 some rays may
                                       have changed, 2 view moved (all changed) */

u8 render_cast_dirty = 1;
static u16 last_vx, last_vy, last_fine;

void render_cast(void)
{
    u8 old = MapSlot1;
    u8 cx = r_viewx >> 8, cy = r_viewy >> 8;

    /* same camera, same map: the rays are still valid */
    if (!render_cast_dirty && r_viewx == last_vx && r_viewy == last_vy
        && r_fine == last_fine)
        return;
    cast_mode = (r_viewx == last_vx && r_viewy == last_vy && r_fine == last_fine) ? 1 : 2;
    render_cast_dirty = 0;
    last_vx = r_viewx; last_vy = r_viewy; last_fine = r_fine;

    MapSlot1 = RD_BANK;
    fxn = (u8)r_viewx;
    fyn = (u8)r_viewy;
    cast_p0 = (u8 *)(CR_TILEMAP + (u16)cx * 64 + cy);

    /* coarse grid every 4th ray, refined where the face changes */
    cast_all();
    MapSlot1 = old;
}

/* ------------------------------------------------------------ drawing */



/* the sprite rays of a tile column: rd_sprites in sprites_a.s, from col_ns / col_lst (made by render_draw) */
extern u8 col_ns[VIEW_TW];
extern u8 col_lst[VIEW_TW][MAXVISIBLE];

void render_draw(void)
{
    u8 old1 = MapSlot1, old2 = slot2_cur;
    u8 i;

    MapSlot1 = RD_BANK;
    slot2_map(TEX_BANK0);           /* cart RAM off: slot 2 = ROM */
    dr_skyv = 0x80 + (r_skycolor << 2);
    dr_gndv = 0x80 + (r_groundcolor << 2);
    i = (r_skycolor << 4) | r_groundcolor;
    if (i != sig_colors)
    {
        sig_colors = i;
        sig_reset = 1;              /* every column looks different now */
        cast_mode = 2;
    }
    /* changed columns: when the rays were cast (the view moved, or something on the map did) the ones whose
     * rays differ from what is on the screen (col_sig, updated in place); none otherwise */
    if (cast_mode)
    {
        if (sig_reset)
        {
            sig_reset = 0;
            memset(col_sig, 0xFF, sizeof(col_sig));
        }
        rd_sig();
    }
    else
        rd_ext(0);
    cast_mode = 0;

    /* sprites: which reach each tile column (rd_buckets, sprites_a.s) */
    if (numvis || spr_any)
        rd_buckets();
    rd_cols();
    MapSlot1 = old1;
    slot2_map(old2);
}


/* ================================================================ camera
 * Everything below is the game side of the view: camera, the projection of
 * statics and actors, the sprite list and the frame.  (Bank 0: it maps the
 * render data banks into slot 1.) */

fixed viewx, viewy, viewsin, viewcos;
s16 viewangle;
s16 centerx = VIEW_W / 2;
const s16 dirangle[9] = { 0, 45, 90, 135, 180, 225, 270, 315, 360 };

#define KV_HALF_256 ((s32)PROJ_KV_HALF_256)   /* half height one tile away, 8.8 (tools/render_gen.py) */
#define KH          ((s32)PROJ_KH)            /* horizontal focal length, pixels */
#define ACTORSIZE_SHIFTED (0x4000L << 6)

/* the projection (sprites_a.s): xf_obj / xf_tile -> xf_vx (centre pixel), xf_sh (half height), xf_depth
 * (objects: 16.16), FALSE when the object can't be seen.  Per frame constants: xf_frame() */
extern s16 xf_c14, xf_s14, xf_vx;
extern s32 xf_k1, xf_k2, xf_depth;
extern u8 xf_sh;
u8 xf_obj(const objtype *ob);
u8 xf_tile(u8 tx, u8 ty);
void add_sprite(u8 sh);
void ds_statics(void);
objtype *ds_nextobj(objtype *ob);

typedef char chk_sprites_a[(sizeof(VisSprite) == 6 && MAXVISIBLE == 32
                            && offsetof(objtype, x) == 12 && offsetof(objtype, y) == 16
                            && offsetof(objtype, state) == 4 && offsetof(objtype, flags) == 6
                            && offsetof(objtype, tilex) == 20 && offsetof(objtype, tiley) == 21
                            && offsetof(objtype, next) == 45 && offsetof(statetype, shapenum) == 1
                            && FL_VISABLE == 8) ? 1 : -1];

/* tile centres: offset of the camera from the centre of its tile, then k1, k2 (see xf_dxy) */
void xf_frame(void)
{
    s16 g0x = (s16)((0x8000L - (s32)(u16)viewx) >> 8);
    s16 g0y = (s16)((0x8000L - (s32)(u16)viewy) >> 8);

    xf_c14 = (s16)(viewcos >> 2);
    xf_s14 = (s16)(viewsin >> 2);
    xf_k1 = smul16(g0x, xf_c14) - smul16(g0y, xf_s14) - ACTORSIZE_SHIFTED;
    xf_k2 = smul16(g0x, xf_s14) + smul16(g0y, xf_c14);
}

s16 CalcRotate (objtype *ob) B
{
    s16 angle, va;

    /* this isn't exactly correct, as it should vary by a trig value, but it
     * is close enough with only eight rotations */
    va = player->angle + (centerx - ob->viewx) / 8;
    angle = (va - 180) - dirangle[ob->dir];
    angle += ANGLES / 16;
    while (angle >= ANGLES) angle -= ANGLES;
    while (angle < 0) angle += ANGLES;
    if (ob->state->rotate == 2)             /* 2 rotation pain frame */
        return 4 * (angle / (ANGLES / 2));  /* separated by 3 (0 and 4) */
    return angle / (ANGLES / 8);
}

/* bearing cull (sprites_a.s): a table of the direction (0..255, 64 = north) to the tile at (dx, dy)
 * from the viewer, dx, dy in -14..14 (render data bank 2) */
extern u8 vang8, vtx, vty;
u8 in_view(u8 tx, u8 ty);

/* is any column of the sprite in front of the walls? */
static boolean sprite_seen(s16 vx, u8 sc)
{
    s16 x0 = vx - sc, x1 = vx + sc;
    u8 c0, c1;
    if (x0 < 0) x0 = 0;
    if (x1 >= VIEW_W) x1 = VIEW_W - 1;
    if (x0 > x1)
        return FALSE;
    for (c0 = (u8)x0 >> 2, c1 = (u8)x1 >> 2; c0 <= c1; c0++)
        if (ray_s[c0] <= sc)
            return TRUE;
    return FALSE;
}

static void DrawScaleds(void)
{
    objtype *obj;
    u8 old = MapSlot1;

    numvis = 0;
    vtx = (u8)(viewx >> 16);
    vty = (u8)(viewy >> 16);
    vang8 = (u8)(((u16)viewangle * 182) >> 8);
    xf_frame();
    MapSlot1 = RD2_BANK;

    ds_statics();

    for (obj = ds_nextobj(player->next); obj; obj = ds_nextobj(obj->next))
    {
        s16 shape = obj->state->shapenum;
        if (!xf_obj(obj))
        {
            obj->viewheight = 0;
            obj->flags &= ~FL_VISABLE;
            continue;
        }
        obj->viewx = xf_vx;
        obj->viewheight = (u16)xf_sh << 1;
        obj->transx = xf_depth;
        if (!sprite_seen(xf_vx, xf_sh))
        {
            obj->flags &= ~FL_VISABLE;
            continue;
        }
        if (!obj->active)
            obj->active = ac_yes;
        if (shape == -1)
            shape = obj->temp1;
        if (obj->state->rotate)
            shape += CalcRotate(obj);
        obj->flags |= FL_VISABLE;
        add_sprite((u8)shape);
    }
    MapSlot1 = old;
}

/* the whole frame: camera, rays, sprites, columns */
u16 game_frames;

#define SPR_X0      (VIEW_TX0 * 8 + VIEW_W / 2 - 64)    /* the weapon canvas: 8 sprites of 16 pixels (8x8 magnified), centred */
#define SPR_Y0      (VIEW_TY0 * 8 + VIEW_H - 80)        /* 5 rows of 16 pixels: the bottom 80 pixels of the view */
#define NOFRAME     0xFF
/* once per frame (ThreeDRefresh): follow gamestate.weapon / weaponframe.  A
 * frame is 40 x 64 texels of the original shape (its bottom 40 rows), shown
 * magnified: only its non-empty 8x8 blocks are sprites, their tiles stored
 * one after the other in row major order (tools/wl_hud.py) */
u8 wp_frame = 0xFF;
void weapon_update(void)
{
    u8 f, r, c, m, n, t;
    const u8 *mk;
    u16 src;
    if (gamestate.weapon < 0 || gamestate.weapon > 3)
    {
        if (wp_frame != NOFRAME)
            weapon_hide();
        return;
    }
    f = gamestate.weapon * 5 + gamestate.weaponframe;
    if (f == wp_frame)
        return;
    wp_frame = f;
    mk = weapon_mask[f];
    for (r = 0, n = 0; r < 5; r++)
        for (m = mk[r]; m; m &= m - 1)
            n++;
    src = 0x8000 + (f % WEAPON_PER_BANK) * (WEAPON_SLOT * 32);
    rom_to_vram_safe(WEAPON_BANK0 + f / WEAPON_PER_BANK, (const void *)src, WEAPON_T0 * 32, (u16)n * 32);
    vdp_set_addr(VRAM_SAT);
    for (r = 0; r < 5; r++)
        for (m = mk[r]; m; m &= m - 1)
        {
            VDPData = SPR_Y0 + 16 * r - 1;
            __asm__("nop\n nop\n nop\n nop");
        }
    VDPData = 0xD0;
    vdp_set_addr(VRAM_SAT + 0x80);
    for (r = 0, t = 0; r < 5; r++)
        for (c = 0, m = mk[r]; c < 8; c++)
            if (m & (1 << c))
            {
                VDPData = SPR_X0 + 16 * c;
                __asm__("nop\n nop\n nop\n nop");
                VDPData = (WEAPON_T0 - 256) + t;
                t++;
                __asm__("nop\n nop\n nop\n nop");
            }
}

#ifdef DEBUG_DET
/* the asm in_view against the C code it replaced, for every offset of a camera and 52 bearings (0: all equal) */
u16 dbg_ivchk;
static u8 ref_in_view(u8 tx, u8 ty)
{
    s8 dx = (s8)(tx - vtx), dy = (s8)(ty - vty);
    u8 a, d, m;
    u8 ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ax > 14 || ay > 14)
        return FALSE;
    if (ax <= 1 && ay <= 1)
        return TRUE;
    a = RD8(RD2_ATAN)[(u8)(dy + 14) * 32 + (u8)(dx + 14)];
    d = a - vang8;
    if (d > 128) d = -d;
    m = ax > ay ? ax : ay;
    return d <= PROJ_FOV8 + 55 / m + 2;
}

void dbg_check_inview(void)
{
    u8 old = MapSlot1, ang;
    s8 dx, dy;
    dbg_ivchk = 0;
    MapSlot1 = RD2_BANK;
    vtx = 30;
    vty = 33;
    for (ang = 0; ang < 252; ang += 5)
    {
        vang8 = ang;
        for (dy = -16; dy <= 16; dy++)
            for (dx = -16; dx <= 16; dx++)
                if (in_view(vtx + dx, vty + dy) != ref_in_view(vtx + dx, vty + dy))
                    dbg_ivchk++;
    }
    MapSlot1 = old;
}
#endif

#ifdef DEBUG_MIRRORS
/* test hooks (make DEBUG=1): smstest reads these with "print" (it only sees system RAM) and
 * "poke"s dbg_wx/wy/wdir/warp to teleport the player */
u8 dbg_tx, dbg_ty, dbg_warp, dbg_wx, dbg_wy, dbg_wdir, dbg_god, dbg_map, dbg_svreq, dbg_uireq, dbg_sndreq;
s16 dbg_ang, dbg_hp, dbg_ammo, dbg_kills, dbg_tics, dbg_ps, dbg_lives;
u16 dbg_score;
void SpawnPlayer(s16 tilex, s16 tiley, s16 dir) B;
void dbg_ui_test(u8 which) B;
#ifdef DEBUG_DET
void dbg_state_hash(void) B;
void dbg_save_test(void) B;
#endif
#endif
void ThreeDRefresh (void)
{
    game_frames++;
#ifdef DEBUG_MIRRORS
    if (dbg_warp)
    {
        dbg_warp = 0;
        SpawnPlayer(dbg_wx, dbg_wy, dbg_wdir);
    }
    if (dbg_map)                /* 1..10: jump to that level (the level change of the game loop) */
    {
        gamestate.mapon = dbg_map - 2;
        dbg_map = 0;
        playstate = ex_completed;
    }
    if (dbg_sndreq)                 /* sound number + 1 */
    {
        u8 sn = dbg_sndreq - 1;
        dbg_sndreq = 0;
        SD_PlaySound(sn);
    }
    if (dbg_uireq)
    {
        u8 w = dbg_uireq;
        dbg_uireq = 0;
        dbg_ui_test(w);
    }
    if (dbg_god)
    {
        gamestate.health = 100;
        gamestate.ammo = 99;
    }
    dbg_tx = player->tilex; dbg_ty = player->tiley; dbg_ang = player->angle;
    dbg_hp = gamestate.health; dbg_ammo = gamestate.ammo; dbg_score = (u16)gamestate.score;
    dbg_kills = gamestate.killcount; dbg_tics = tics; dbg_ps = playstate; dbg_lives = gamestate.lives;
#ifdef DEBUG_DET
    dbg_state_hash();
    if (dbg_svreq)
    {
        dbg_svreq = 0;
        dbg_save_test();
    }
#endif
#endif
    viewangle = player->angle;
    viewsin = fsin(viewangle);
    viewcos = fcos(viewangle);
    viewx = player->x - FixedByFrac(FOCALLENGTH, viewcos);
    viewy = player->y + FixedByFrac(FOCALLENGTH, viewsin);

    r_viewx = (u16)(viewx >> 8);
    r_viewy = (u16)(viewy >> 8);
    r_fine = viewangle * 10;
    render_set_colors(1, 0);

    render_cast();
    DrawScaleds();
    if (fizzlein)
        fizzle_begin();
    render_draw();
    if (fizzlein)
    {
        fizzlein = FALSE;
        FizzleFade();
        set_timecount(get_timecount());
    }
    weapon_update();
#ifdef DEBUG_DET
    /* a frame takes at least two console frames, however cheap it was: the test script changes the pad
     * between console frames, and has to do so before the next frame polls it, in every build */
    wait_vblank();
    wait_vblank();
#endif
}
