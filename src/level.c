/* level.c - bank 0: the current level's ROM data (area map, info plane, doors)
 * and the constant tables the game code of every bank shares */
#include "game/def.h"

/* cart RAM layout (cartram.h, symbols in cartsyms.s) */
typedef char chk_a[(CR_TILEMAP == 0x8000 && CR_ACTORAT == 0x9000 && CR_OBJLIST == 0xA000 && CR_STATLIST == 0xB650 && CR_DOORLIST == 0xBC00 && CR_DOORPOS16 == 0xBB80) ? 1 : -1];
typedef char chk_obj[(sizeof(objtype) * MAXACTORS <= CR_STATLIST - CR_OBJLIST) ? 1 : -1];
typedef char chk_objsize[(sizeof(objtype) == CR_OBJSIZE) ? 1 : -1];
typedef char chk_stat[(sizeof(statobj_t) * MAXSTATS <= CR_DOORPOS - CR_STATLIST) ? 1 : -1];
typedef char chk_door[(sizeof(doorobj_t) * MAXDOORS <= CR_SCRATCH - CR_DOORLIST) ? 1 : -1];

u8 lvl_bank;
u16 lvl_addr;
static u16 lvl_o_area, lvl_o_obj, lvl_o_door, lvl_nobj;
u8 aovr_n;
/* AreaOf of the last tile asked for: the same tile is asked again and again (every step of the player, the
 * walkers), and the area map is run length coded.  Dropped when the level or the overrides change */
static s16 ca_x, ca_y;
static u8 ca_v, ca_ok;
u8 aovr[40][3];

/* tile -> sprite for the static objects (index = statinfo type) */
const u8 statinfo_pic[NUMSTATTYPES] =
{
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 26
};
const s8 statinfo_type[NUMSTATTYPES] =
{
    dressing, block, block, block, dressing, block, bo_alpo, block, block, dressing,
    block, block, block, block, dressing, dressing, block, block, block, dressing,
    bo_key1, bo_key2, block, dressing, bo_food, bo_firstaid, bo_clip, bo_machinegun, bo_chaingun, bo_cross,
    bo_chalice, bo_bible, bo_crown, bo_fullheal, bo_gibs, block, block, block, bo_gibs, block,
    block, dressing, dressing, dressing, dressing, block, block, dressing, bo_clip2
};

void SetAreaOverride(s16 x, s16 y, u8 area)
{
    u8 i;
    ca_ok = 0;
    for (i = 0; i < aovr_n; i++)
        if (aovr[i][0] == x && aovr[i][1] == y)
        {
            aovr[i][2] = area;
            return;
        }
    if (aovr_n < 40)
    {
        aovr[aovr_n][0] = x;
        aovr[aovr_n][1] = y;
        aovr[aovr_n][2] = area;
        aovr_n++;
    }
}

void AreaCacheReset(void)
{
    ca_ok = 0;
}

u8 AreaOf(s16 x, s16 y)
{
    u8 i, old, n, v;
    s16 ys = y;
    const u8 *base, *p;
    if (ca_ok && x == ca_x && y == ca_y)
        return ca_v;
    for (i = 0; i < aovr_n; i++)
        if (aovr[i][0] == x && aovr[i][1] == y)
            return aovr[i][2];
    old = MapSlot1;
    MapSlot1 = lvl_bank;
    base = (const u8 *)(lvl_addr - 0x4000 + lvl_o_area);
    p = base + (base[2 * x] | (base[2 * x + 1] << 8));
    for (;;)
    {
        n = p[0];
        v = p[1];
        p += 2;
        if (y < n)
            break;
        y -= n;
    }
    MapSlot1 = old;
    ca_ok = 1;
    ca_x = x;
    ca_y = ys;
    ca_v = v;
    return v;
}

u16 InfoPlane(s16 x, s16 y)
{
    u8 i, old, code = 0;
    s16 lo, hi, mid;
    u16 key = ((u16)y << 6) | (u16)x, k;
    const u8 *base, *e;
    for (i = 0; i < info_clr_n; i++)
        if (info_clr_x[i] == x && info_clr_y[i] == y)
            return 0;
    old = MapSlot1;
    MapSlot1 = lvl_bank;
    base = (const u8 *)(lvl_addr - 0x4000 + lvl_o_obj + 2);
    lo = 0;
    hi = (s16)lvl_nobj - 1;
    while (lo <= hi)
    {
        mid = (lo + hi) >> 1;
        e = base + mid * 3;
        k = e[0] | (e[1] << 8);
        if (k == key)
        {
            code = e[2];
            break;
        }
        if (k < key)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    MapSlot1 = old;
    return code;
}

/* select level n: tile map into cart RAM (which must be mapped), tables */
void LevelLoad(u8 n)
{
    u8 hdr[8];
    u16 o_t;
    u8 cnt[2];
    lvl_bank = map_bank[n];
    lvl_addr = map_addr[n];
    rom_read(lvl_bank, lvl_addr, hdr, 8);
    o_t = hdr[0] | (hdr[1] << 8);
    lvl_o_area = hdr[2] | (hdr[3] << 8);
    lvl_o_obj = hdr[4] | (hdr[5] << 8);
    lvl_o_door = hdr[6] | (hdr[7] << 8);
    rom_read(lvl_bank, lvl_addr + lvl_o_obj, cnt, 2);
    lvl_nobj = cnt[0] | (cnt[1] << 8);
    rom_read(lvl_bank, lvl_addr + o_t, (void *)CR_TILEMAP, 4096);
    aovr_n = 0;
    ca_ok = 0;
}

u16 LevelObjCount(void)
{
    return lvl_nobj;
}

/* object i (y major order): tile key (y * 64 + x) and code */
u16 LevelObj(u16 i, u8 *code)
{
    u8 e[3];
    rom_read(lvl_bank, lvl_addr + lvl_o_obj + 2 + i * 3, e, 3);
    *code = e[2];
    return e[0] | (e[1] << 8);
}

u8 LevelDoorCount(void)
{
    u8 n;
    rom_read(lvl_bank, lvl_addr + lvl_o_door, &n, 1);
    return n;
}

void LevelDoor(u8 i, u8 *dst6)
{
    rom_read(lvl_bank, lvl_addr + lvl_o_door + 1 + i * 6, dst6, 6);
}

/* the level's ROM address of the plain 4096 byte tile map (restore) */
u16 LevelTileOfs(void)
{
    u8 hdr[2];
    rom_read(lvl_bank, lvl_addr, hdr, 2);
    return hdr[0] | (hdr[1] << 8);
}
