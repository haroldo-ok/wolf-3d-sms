/* render.h - SMS 3-D view */
#ifndef RENDER_H
#define RENDER_H
#include "hw.h"

/* the view's geometry (VIEW_TW, VIEW_TH, VIEW_W, VIEW_CY, VIEW_TX0/TY0, TILE_*) comes from tools/render_gen.py through
 * gen/assets.h: a 192 x 96 view of 24 x 12 tiles, 48 rays of 4 pixels, centred in the top 12 rows of the screen */
#include "assets.h"
#define VIEW_W     (VIEW_TW * 8)

#define MAXVISIBLE  32

typedef struct {
    s16 x0;             /* left pixel */
    u16 width;          /* pixels */
    u8  s;              /* half height */
    u8  shape;          /* shape index (pic - FIRSTSCALEPIC) */
} VisSprite;

extern u16 r_viewx, r_viewy;    /* 8.8 camera position */
extern u16 r_fine;              /* fine view angle 0..3599 */
extern u8  r_animframe;
extern u8  r_skycolor, r_groundcolor;
extern u8  cr_pwpos;            /* moving push wall: offset 0..63 (1/64 tile) */
extern u8  ray_s[];             /* z buffer (half heights) */
extern VisSprite vissprites[MAXVISIBLE];
extern u8  numvis;
extern u8  render_cast_dirty;      /* set when the tile map changes */
extern u8  nt_hold;             /* nonzero: render_draw leaves the nametable alone */
extern u8  nt_state[];          /* per view cell: 0 sky, 1 ground, 2 own tile */

void render_init(void);         /* view tiles / nametable, call with display off */
void render_invalidate(void);   /* force full redraw of the nametable state */
void render_set_colors(u8 sky, u8 ground);
void render_cast(void);         /* rays (cart RAM must be in slot 2) */
void render_draw(void);         /* draw view from rays + vissprites */
u16  mulhi16(u16 a, u16 b);     /* (a*b) >> 16 */

#endif
