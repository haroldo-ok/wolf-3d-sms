/* cartram.h - layout of cartridge RAM half 0 (mapped in slot 2 during play) */
#ifndef CARTRAM_H
#define CARTRAM_H

#define MAPSIZE   64

#define CR_TILEMAP   0x8000      /* u8 [64][64], x major: 0 floor, 1..63 wall (|0x40 beside a door),
                                    0x80|n door n, 0xC0|n moving push wall */
#define CR_ACTORAT   0x9000      /* u8 [64][64]: 0 nothing, 1..63 wall/blocker, 0x80|n door, 0x40|id actor */
#define CR_OBJLIST   0xA000      /* objtype [MAXACTORS] */
#define CR_OBJSIZE   50          /* sizeof(objtype): fast.s multiplies by it (level.c checks it) */
#define CR_STATLIST  0xB650      /* statobj_t [MAXSTATS], 3 bytes each */
#define CR_DOORPOS   0xBB00      /* u8 [64]: door position / 256 (the caster reads it) */
#define CR_DOORPAGE  0xBB40      /* u8 [64]: door picture base (the caster reads it) */
#define CR_DOORPOS16 0xBB80      /* u16 [64]: door position */
#define CR_DOORLIST  0xBC00      /* doorobj_t [64], 9 bytes each */
#define CR_SCRATCH   0xBE40      /* 448 bytes of work space */
#define CR_END       0xC000

#define CR_DOORPOSB ((u8 *)CR_DOORPOS)
#define CR_DOORPAGEB ((u8 *)CR_DOORPAGE)

#endif
