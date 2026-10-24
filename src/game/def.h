/* def.h - Wolfenstein 3D (shareware, episode 1) for the Sega Master System.
 * Game definitions shared by the files in src/game (port of WL_DEF.H, the Genesis
 * port's wl_def.h and wl_proto.h).  int is 16 bit here: positions are s32. */
#ifndef DEF_H
#define DEF_H

#include "hw.h"
#include "mathz.h"
#include "assets.h"
#include "cartram.h"
#include "render.h"
#include "plat.h"

#ifndef B
#define B __banked
#endif

typedef u8 boolean;
typedef u8 byte;
typedef u16 word;
#define TRUE  1
#define FALSE 0

#define SIGN(x)         ((x) > 0 ? 1 : -1)
#define ABS(x)          ((x) > 0 ? (x) : -(x))
#ifndef abs
#define abs(x)          ((x) < 0 ? -(x) : (x))
#endif
#define LABS(x)         ((s32)(x) > 0 ? (x) : -(x))

#define NUMENEMIES      22
#define ICONARROWS      90
#define MAXACTORS       112
#define MAXSTATS        400
#define MAXDOORS        64
#define MAXWALLTILES    64

#define PUSHABLETILE    98
#define EXITTILE        99
#define AREATILE        107
#define NUMAREAS        37
#define ELEVATORTILE    21
#define AMBUSHTILE      106
#define ALTELEVATORTILE 107

#define EXTRAPOINTS     40000
#define PLAYERSPEED     3000
#define RUNSPEED        6000
#define PLAYERSIZE      MINDIST
#define MINACTORDIST    0x10000l

#define GLOBAL1         (1l << 16)
#define TILEGLOBAL      GLOBAL1
#define PIXGLOBAL       (GLOBAL1 / 64)
#define TILESHIFT       16
#define ANGLES          360
#define ANGLEQUAD       (ANGLES / 4)
#define FINEANGLES      3600
#define ANG90           (FINEANGLES / 4)
#define ANG180          (ANG90 * 2)
#define ANG270          (ANG90 * 3)
#define ANG360          (ANG90 * 4)
#define VANG90          (ANGLES / 4)
#define VANG180         (VANG90 * 2)
#define VANG270         (VANG90 * 3)
#define VANG360         (VANG90 * 4)
#define MINDIST         (0x5800l)
#define FOCALLENGTH     (0x5800l)

#define NORTH   0
#define EAST    1
#define SOUTH   2
#define WEST    3
#define STARTAMMO   8

#define VIEWW   VIEW_W

/* HUD numbers / faces (hud.c) */
enum { HUD_FLOOR, HUD_SCORE, HUD_LIVES, HUD_HEALTH, HUD_AMMO };
#define FACE_DEAD    21
#define FACE_GATLING 22
#define NUMFACES     23

#define UNSIGNEDSHIFT   8
#define TURNTICS        10
#define SPDPATROL       512
#define SPDDOG          1500

/* object flags */
#define FL_SHOOTABLE    1
#define FL_BONUS        2
#define FL_NEVERMARK    4
#define FL_VISABLE      8
#define FL_ATTACKMODE   16
#define FL_FIRSTATTACK  32
#define FL_AMBUSH       64
#define FL_NONMARK      128

#define DOORWALL        98          /* first door wall page */

typedef enum { di_north, di_east, di_south, di_west } controldir_t;
typedef enum { dr_normal, dr_lock1, dr_lock2, dr_lock3, dr_lock4, dr_elevator } door_t;
typedef enum { ac_badobject = -1, ac_no, ac_yes, ac_allways } activetype;

typedef enum
{
    nothing, playerobj, inertobj, guardobj, officerobj, ssobj, dogobj,
    bossobj, schabbobj, fakeobj, mechahitlerobj, mutantobj, needleobj,
    fireobj, bjobj, ghostobj, realhitlerobj, gretelobj, giftobj, fatobj,
    rocketobj, spectreobj, angelobj, transobj, uberobj, willobj, deathobj,
    hrocketobj, sparkobj
} classtype;

typedef enum
{
    dressing, block, bo_gibs, bo_alpo, bo_firstaid, bo_key1, bo_key2,
    bo_key3, bo_key4, bo_cross, bo_chalice, bo_bible, bo_crown, bo_clip,
    bo_clip2, bo_machinegun, bo_chaingun, bo_food, bo_fullheal, bo_25clip,
    bo_spear
} stat_t;

typedef enum
{
    east, northeast, north, northwest, west, southwest, south, southeast, nodir
} dirtype;

typedef enum
{
    en_guard, en_officer, en_ss, en_dog, en_boss, en_schabbs, en_fake,
    en_hitler, en_mutant, en_blinky, en_clyde, en_pinky, en_inky, en_gretel,
    en_gift, en_fat, en_spectre, en_angel, en_trans, en_uber, en_will,
    en_death
} enemy_t;

struct objstruct;
/* states live in bank 0 (gen/states.c); think / action are routine numbers */
typedef struct statestruct
{
    u8 rotate;
    s16 shapenum;               /* -1 = take from ob->temp1 */
    s16 tictime;
    u8 think;
    u8 action;
    const struct statestruct *next;
} statetype;

/* a static object: 3 bytes.  type indexes statinfo[] (0xFF: removed) */
typedef struct
{
    u8 tilex, tiley;
    u8 type;
} statobj_t;
#define STAT_REMOVED 0xFF

enum { dr_open, dr_closed, dr_opening, dr_closing };
typedef struct
{
    u8 tilex, tiley;
    u8 vertical;
    u8 lock;
    u8 action;
    u8 area1, area2;
    s16 ticcount;
} doorobj_t;

typedef struct objstruct
{
    s8 active;
    s16 ticcount;
    u8 obclass;
    const statetype *state;
    u8 flags;
    s32 distance;
    u8 dir;
    s32 x, y;
    u8 tilex, tiley;
    u8 areanumber;
    s16 viewx;
    u16 viewheight;
    s32 transx;
    s16 angle;
    s16 hitpoints;
    s32 speed;
    s16 temp1, temp2, temp3;
    struct objstruct *next, *prev;
    u8 enc;                     /* this slot's actorat code (set once by InitActorList) */
} objtype;

enum { bt_nobutton = -1, bt_attack = 0, bt_strafe, bt_run, bt_use,
       bt_readyknife, bt_readypistol, bt_readymachinegun, bt_readychaingun };
#define NUMBUTTONS 8

typedef enum { wp_knife, wp_pistol, wp_machinegun, wp_chaingun } weapontype;
enum { gd_baby, gd_easy, gd_medium, gd_hard };

typedef struct
{
    s16 difficulty;
    s16 mapon;
    s32 oldscore, score, nextextra;
    s16 lives;
    s16 health;
    s16 ammo;
    s16 keys;
    weapontype bestweapon, weapon, chosenweapon;
    s16 faceframe;
    s16 attackframe, attackcount, weaponframe;
    s16 episode, secretcount, treasurecount, killcount,
        secrettotal, treasuretotal, killtotal;
    s32 TimeCount;
    s32 killx, killy;
    boolean victoryflag;
} gametype;

typedef enum
{
    ex_stillplaying, ex_completed, ex_died, ex_warped, ex_resetgame,
    ex_loadedgame, ex_victorious, ex_abort, ex_demodone, ex_secretlevel
} exit_t;

#include "states.h"

/* ----------------------------------------------------------- globals */
extern gametype gamestate;
extern exit_t playstate;
extern boolean madenoise;
extern objtype objlist[MAXACTORS];            /* cart RAM, level.c */
extern objtype *newobj, *player, *lastobj, *objfreelist, *killerobj;
extern statobj_t statobjlist[MAXSTATS];
extern statobj_t *laststatobj;
extern doorobj_t doorobjlist[MAXDOORS];
extern doorobj_t *lastdoorobj;
extern u16 doorposition[MAXDOORS];

extern u8 tilemap[MAPSIZE][MAPSIZE];
extern u8 actorat[MAPSIZE][MAPSIZE];
/* actorat holds 0 (nothing), a wall/blocking value (1..0x3F), a door
 * (0x80|n) or an actor: 0x40|id for id<64, 0xC0|(id-64) for id>=64 */
#define ACT_ISACTOR(v)      ((v) & 0x40)
#define act_enc(o)          ((o)->enc)
objtype *act_dec(u8 v);                     /* fast.s (bank 0): 0 when v is not an actor */
s32 mul_tics(u8 t, u16 v);                  /* fast.s: t * v */
extern u8 pick_valid;                       /* wl_agent.c: CheckPickups must scan again */
extern u8 sc_valid;                         /* sprites_a.s: the statics near the camera must be listed again */
extern u8 exit_valid;                       /* wl_agent.c: Thrust must look for the exit tile again */
#define STATICS_CHANGED()   (pick_valid = 0, sc_valid = 0, exit_valid = 0)
#define ACTORAT(x, y)       act_dec(actorat[x][y])

/* the level's ROM data (level.c, bank 0) */
void LevelLoad(u8 n);
u16  LevelObjCount(void);
u16  LevelObj(u16 i, u8 *code);      /* returns the tile key y * 64 + x */
u8   LevelDoorCount(void);
void LevelDoor(u8 i, u8 *dst6);
void rom_read(u8 bank, u16 addr, void *dst, u16 len);
/* area numbers of the level (ROM) */
u8 AreaOf(s16 x, s16 y);                    /* bank 0: 0..36, 0xFF not floor; |0x40 ambush */
void SetAreaOverride(s16 x, s16 y, u8 area);  /* a vacated push wall tile */
extern u8 aovr_n;
#define AREA(x, y)          (AreaOf(x, y) & 0x3F)
u16 InfoPlane(s16 x, s16 y);                /* bank 0: plane 1 code of a tile, 0 none */
#define MAPSPOT0(x, y)      (AREATILE + AREA(x, y))

extern fixed viewx, viewy;
extern s16 viewangle;
extern fixed viewsin, viewcos;
extern s16 centerx;
extern u16 pwallpos, pwallstate, pwallx, pwally;
extern s16 pwalldir;
extern boolean areabyplayer[NUMAREAS];
extern s16 tics;
extern boolean loadedgame;

#define sintable_unused 0
fixed fsin(s16 a);                          /* 16.16 sine, a in 0..ANGLES-1 */
fixed fcos(s16 a);

#define MAXTICS         10

/* ------------------------------------------------------------ enemies */
extern const dirtype opposite[9];
extern const dirtype diagonal[9][9];

/* wl_main */
u16 US_RndT(void) B;
void US_InitRndT(boolean randomize) B;
void Quit(const char *error);

/* wl_main / wl_game */
void NewGame(s16 difficulty, s16 episode) B;
extern boolean startgame, loadedgame, ingame;
void DrawPlayScreen(void) B;
void Died(void) B;

/* wl_act1 */
void InitDoorList(void) B;
void InitStaticList(void) B;
void SpawnStatic(s16 tilex, s16 tiley, s16 type) B;
void SpawnDoor(u8 tilex, u8 tiley, u8 vertical, u8 lock, u8 a1, u8 a2) B;
void MoveDoors(void) B;
void MovePWalls(void) B;
void OpenDoor(s16 door) B;
void PlaceItemType(s16 itemtype, s16 tilex, s16 tiley) B;
void PushWall(s16 checkx, s16 checky, s16 dir) B;
void OperateDoor(s16 door) B;
void InitAreas(void) B;
void ConnectAreas(void) B;
extern u8 info_clr_x[], info_clr_y[], info_clr_n;
extern s16 doornum;
extern u8 pwj_n;
extern u8 pwj[][3];
extern u8 doors_busy;                       /* wl_act1.c: doors that are not closed */
#define NUMSTATTYPES 49
extern const u8 statinfo_pic[NUMSTATTYPES];      /* sprite - SPR_STAT_0 */
extern const s8 statinfo_type[NUMSTATTYPES];
#define STAT_SHAPE(t) (SPR_STAT_0 + statinfo_pic[t])

/* wl_state */
void SpawnNewObj(u16 tilex, u16 tiley, const statetype *state) B;
void NewState(objtype *ob, const statetype *state) B;
boolean TryWalk(objtype *ob) B;
void SelectChaseDir(objtype *ob) B;
void SelectDodgeDir(objtype *ob) B;
void SelectRunDir(objtype *ob) B;
void MoveObj(objtype *ob, s32 move) B;
boolean SightPlayer(objtype *ob) B;
/* true when SightPlayer(ob) would return FALSE at once with no side effect (not reacting yet, the area is not
 * connected to the player's): the banked call can be skipped */
#define SIGHT_SKIP(ob)  (!((ob)->flags & FL_ATTACKMODE) && !(ob)->temp2 && !areabyplayer[(ob)->areanumber])
void KillActor(objtype *ob) B;
void DamageActor(objtype *ob, u16 damage) B;
boolean CheckLine(objtype *ob) B;
boolean CheckSight(objtype *ob) B;
void FirstSighting(objtype *ob) B;
void DropItem(stat_t itemtype, s16 tilex, s16 tiley) B;

/* wl_act2 */
void SpawnStand(enemy_t which, s16 tilex, s16 tiley, s16 dir) B;
void SpawnPatrol(enemy_t which, s16 tilex, s16 tiley, s16 dir) B;
void SpawnDeadGuard(s16 tilex, s16 tiley) B;
void SpawnBoss(s16 tilex, s16 tiley) B;
void A_DeathScream(objtype *ob) B;
void A_StartDeathCam(objtype *ob) B;
s16 AimAngle(s32 dy, s32 dx) B;

/* wl_agent */
extern boolean running;
extern s32 thrustspeed;
extern u16 plux, pluy;
extern s16 anglefrac;
extern s16 facecount;
void SpawnPlayer(s16 tilex, s16 tiley, s16 dir) B;
void DrawFace(void) B;
void DrawStatusBar(void) B;
void DrawHealth(void) B;
void TakeDamage(s16 points, objtype *attacker) B;
void HealSelf(s16 points) B;
void DrawLevel(void) B;
void DrawLives(void) B;
void GiveExtraMan(void) B;
void DrawScore(void) B;
void GivePoints(s32 points) B;
void DrawWeapon(void) B;
void DrawKeys(void) B;
void GiveWeapon(s16 weapon) B;
void DrawAmmo(void) B;
void GiveAmmo(s16 ammo) B;
void GiveKey(s16 key) B;
void GetBonus(statobj_t *check) B;
void CheckPickups(void) B;
void Thrust(s16 angle, s32 speed) B;

/* wl_play */
void CalcTics(void) B;
void StartMusic(void) B;
void StopMusic(void) B;
extern boolean godmode, noclip;
extern boolean buttonstate[NUMBUTTONS];
extern s16 controlx, controly;
extern boolean singlestep;
extern boolean buttonheld[NUMBUTTONS];
extern s16 shootdelta;
void SpawnBJVictory(void) B;
void InitActorList(void) B;
void GetNewActor(void) B;
void RemoveObj(objtype *gone) B;
void PollControls(void) B;
void PlayLoop(void) B;
void StartDamageFlash(s16 damage) B;
void StartBonusFlash(void) B;
void FinishPaletteShifts(void) B;
void InitRedShifts(void) B;

/* wl_game */
void DeathCamIntro(void) B;
extern boolean fizzlein;
void SetupGameLevel(void) B;
void ScanInfoPlane(void) B;
void GameLoop(void) B;
void PlaySoundLocGlobal(u16 s, s32 gx, s32 gy) B;
void UpdateSoundLoc(void) B;
#define PlaySoundLocTile(s, tx, ty) \
    PlaySoundLocGlobal(s, (((s32)(tx) << TILESHIFT) + (1L << (TILESHIFT - 1))), (((s32)(ty) << TILESHIFT) + (1L << (TILESHIFT - 1))))
#define PlaySoundLocActor(s, ob)    PlaySoundLocGlobal(s, (ob)->x, (ob)->y)

/* render.c (bank 0): camera, projection, the sprite list */
void ThreeDRefresh(void);
s16 CalcRotate(objtype *ob) B;
void TransformActor(objtype *ob) B;
#define RD8(a)  ((const u8 *)(a))
extern const s16 dirangle[9];

#endif
