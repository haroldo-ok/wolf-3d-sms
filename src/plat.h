/* plat.h - SMS platform layer used by the game code.
 *
 * Bank 0 (plain calls): snd.c, level.c, mathz.c, plat0.c: sound player, ROM
 *   data access, math, timer.
 * Banked (platform banks): HUD, video modes, input, menus, saves.
 */
#ifndef PLAT_H
#define PLAT_H
#include "hw.h"

#ifndef B
#define B __banked
#endif

/* ----------------------------------------------------------- bank 0 */
u32  get_timecount(void);
void set_timecount(u32 t);

/* sound: the player is in snd.c (bank 0), the game's interface is banked (snd_ctl.c) */
extern u8 SoundOn, MusicOn, DigiOn;
void SD_Startup(void);
u8   SD_PlaySound(u16 sound) B;
void SD_StopSound(void) B;
void SD_WaitSoundDone(void) B;
u16  SD_SoundPlaying(void) B;
void SD_StartMusic(u16 song) B;
void SD_MusicOff(void) B;
void SD_FadeOutMusic(void) B;

/* ------------------------------------------------------------ banked */
/* HUD (status area below the view) */
void hud_load(void) B;                  /* tiles + nametable of the play screen */
void hud_number(u8 which, s32 value) B; /* 0 floor 1 score 2 lives 3 health 4 ammo */
void hud_face(u8 pic) B;                /* see FACE_* */
void hud_weapon(u8 weapon) B;
void hud_keys(u8 keys) B;
void hud_messages(void) B;
void hud_flush(void) B;

/* video */
void vid_init_game(void) B;
void vid_set_palette_black(void) B;
void vid_fade_out(void) B;
void vid_fade_in(void) B;
void vid_flash(u8 r, u8 g, u8 b, u8 level) B;      /* tint the palette (0..3), 0 = normal */
void vid_wait(u16 frames) B;
void vid_set_ceiling(u8 map_ceil) B;      /* palette entry of the ceiling for this level */
void fizzle_begin(void) B;
void FizzleFade(void) B;

/* input */
typedef struct
{
    u8 held, pressed;
} PadState;
extern u8 pad_now, pad_last;
void IN_Update(void) B;
void IN_ClearKeys(void) B;
u8   IN_CheckAck(void) B;
void IN_Ack(void) B;
u8   IN_UserInput(u32 delay) B;

/* weapon sprite (hardware sprites over the view) */
void weapon_load(void) B;
void weapon_show(u8 weapon, u8 frame) B;
void weapon_hide(void) B;
void weapon_update(void);                 /* bank 0 */
void rom_read(u8 bank, u16 addr, void *dst, u16 len);   /* bank 0 */
void VW_ColorBorder(u8 color) B;
void vid_set_palette(const u8 *p0, const u8 *p1) B;
void vid_set_palette_normal(void) B;
void nt_fill(u16 tile) B;

/* menus, intermission, saves (banked) */
void US_ControlPanel(void) B;
void Inter_GetPsyched(void) B;
void LevelCompleted(void) B;
void Victory(void) B;
void CheckHighScore(s32 score, u16 other) B;
void ViewScores(void) B;
void Inter_FadeToColor(u8 color) B;
extern u8 screenfaded;

#endif
