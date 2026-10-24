/* ui.h - the front end: full screens made of tiles (tools/wl_pics.py), menus, intermission, scores */
#ifndef UI_H
#define UI_H
#include "game/def.h"

#define CURS_T0 436                     /* VRAM tile of the cursor sprite tiles (tools/wl_scr.py) */

typedef struct
{
    u16 ntiles, tiles_off, nt_off;
    u8 pal[32];
    u16 font_t0, dig_t0;
    u8 nitems;
    u16 items_off;
    u8 nvars;
    u16 vars_off, curs_off;
    u8 bg_slot, flags;
} ScrHdr;
typedef struct { u8 col, row, w, h, cx, cy, flags, pad; } ScrItem;
typedef struct { u8 col, row, w, h, n; u16 off; } ScrVar;

/* screens (scr.c) */
void scr_load(u8 id) B;                 /* tiles, name table, palettes (black); display on */
void scr_show(u8 id) B;                 /* fade out, load, fade in */
void scr_leave(void) B;                 /* fade out, give slot 2 back */
void ui_text(u8 col, u8 row, const char *s, u8 pal) B;
void ui_clear(u8 col, u8 row, u8 w, u8 h) B;       /* the cells as the screen was made */
void ui_pal(u8 col, u8 row, u8 w, u8 h, u8 pal) B; /* palette of the cells (0: normal, 1: highlighted ink) */
void ui_number(u8 col, u8 row, u8 w, s32 v, u8 blank) B;   /* big digits, right aligned in w cells */
void ui_time(u8 col, u8 row, u16 min, u8 sec) B;
void ui_variant(u8 rec, u8 v) B;
void ui_cursor(u8 x, u8 y, u8 frame) B;
void ui_cursor_off(void) B;
extern u8 scr_cur;
extern const ScrHdr *scr_h;

/* menus (menu.c) */
s8 menu_run(u8 start, void (*onmove)(u8)) B;       /* the chosen item, -1: back */
u8 ui_wait(u16 tics) B;                 /* TRUE: a button was pressed before the time was up */
void ui_itoa(char *buf, s32 v) B;
void ui_pad(char *buf, s32 v, u8 w) B;  /* right aligned in w characters */

/* save.c: 3 slots and the score table in cartridge RAM page 1 */
typedef struct { u8 floor, diff, lives; s32 score; } SaveInfo;
u8 Save_Info(u8 slot, SaveInfo *si) B;  /* TRUE: the slot holds a game */
u8 Save_Write(u8 slot) B;
u8 Save_Read(u8 slot) B;
void Scores_Load(void) B;
void Scores_Store(void) B;

/* inter.c */
extern u8 ratio_kill[8], ratio_secret[8], ratio_treasure[8];
extern u16 ratio_time[8];
void title_loop(void) B;                /* the attract loop and the menu; returns when a game starts */
void Quit_ToTitle(void) B;

#endif
