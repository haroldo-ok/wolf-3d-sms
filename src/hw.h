/* hw.h - minimal Sega Master System hardware layer */
#ifndef HW_H
#define HW_H

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef signed int s16;
typedef unsigned long u32;
typedef signed long s32;

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ports */
__sfr __at 0xBE VDPData;
__sfr __at 0xBF VDPCtrl;
__sfr __at 0x7E VCounter;
__sfr __at 0x7F PSGPort;
__sfr __at 0xDC IOPortA;
__sfr __at 0xDD IOPortB;
__sfr __at 0x3F IOCtrl;

/* mapper */
volatile __at(0xFFFC) u8 MapCtrl;
volatile __at(0xFFFD) u8 MapSlot0;
volatile __at(0xFFFE) u8 MapSlot1;
volatile __at(0xFFFF) u8 MapSlot2;

#define MAP_CARTRAM  0x08
#define MAP_CARTRAM1 0x0C   /* second 16 KiB cart RAM bank */

/* pad bits (active high after inversion) */
#define PAD_UP     0x01
#define PAD_DOWN   0x02
#define PAD_LEFT   0x04
#define PAD_RIGHT  0x08
#define PAD_1      0x10
#define PAD_2      0x20
#define PAD_PAUSE  0x40     /* synthesised from the NMI */

/* VRAM layout */
#define VRAM_NAMETABLE 0x3800
#define VRAM_SAT       0x3F00

extern volatile u8  frame_count;     /* increments every vblank */
extern volatile u32 time_count;      /* 70 Hz game clock (TimeCount) */
/* the pad is sampled at every vblank; a game frame takes 0.2 - 0.4 s here, so pad_read() reports what
 * happened since its last call: pad_new = buttons pressed (edges), pad_f* = how long each direction was
 * down (0..256 = the whole time) */
extern volatile u8  pad_edge, pad_prev, pad_n;
extern volatile u8  pad_cnt[4];
extern u8  pad_new;
extern u16 pad_f_up, pad_f_down, pad_f_left, pad_f_right;
extern volatile u8  pause_hit;       /* NMI latch */
extern volatile u8  vdp_status_last;

void hw_init(void);
void vdp_reg(u8 reg, u8 val);
void vdp_display(u8 on);
void vdp_set_addr(u16 addr);         /* write address (VRAM write mode) */
void vdp_set_read(u16 addr);
void vdp_set_cram(u8 index);
void vram_fill(u16 addr, u8 val, u16 len);       /* display off / vblank only */
void vram_write(u16 addr, const u8 *src, u16 len);/* display off only (fast) */
void vram_write_safe(u16 addr, const u8 *src, u16 len); /* paced, any time */
void vram_write_nt(u16 addr, const u16 *src, u8 count); /* paced words */
void vram_write32(u16 addr, const u8 *src);   /* 32 bytes, paced */
void vram_read32(u16 addr, u8 *dst);          /* 32 bytes, paced */
void vram_read(u16 addr, u8 *dst, u16 len);      /* paced */
void vram_put_word(u16 addr, u16 w);
void cram_load(const u8 *pal, u8 first, u8 n);
void cram_set(u8 index, u8 val);
void wait_vblank(void);
void wait_frames(u16 n);
u8   pad_read(void);

/* banked data helpers (all live in bank 0) */
u8   rom_bank_slot1(u8 bank);        /* returns previous */
void rom_copy(u8 bank, const void *src, void *dst, u16 len); /* via slot 2 */
void rom_to_vram(u8 bank, const void *src, u16 vaddr, u16 len); /* via slot 2, display off */
void rom_to_vram_safe(u8 bank, const void *src, u16 vaddr, u16 len);
extern u8 slot2_cur;                 /* what is mapped in slot 2 */
void slot2_map(u8 bank);             /* bank 0xFF = cart RAM 0, 0xFE = cart RAM 1 */
#define SLOT2_CARTRAM  0xFF
#define SLOT2_CARTRAM1 0xFE

/* debug */
void dbg_byte(u8 v);

#endif
