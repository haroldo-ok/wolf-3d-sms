#ifndef SND_H
#define SND_H
#include "hw.h"

typedef struct
{
    u8 on, bank;
    const u8 *p;
    u8 wait, pri;
    u8 pad[2];
    u16 per[3];                         /* tone periods */
    u8 vol[4];                          /* attenuations, 15 = off */
    u8 nctl, pad2;                      /* noise control */
} Voice;

extern Voice mus, sfx;
extern u16 cur_per[3];
extern u8 cur_vol[4], cur_nctl;
extern u8 snd_ticks, pcm_on;
extern u8 pcm_buf[];
extern const u8 *pcm_ptr, *pcm_src;
extern u8 pcm_bank;
extern u16 pcm_left;

void SD_Startup(void);
void snd_frame(void);                   /* the frame interrupt (hw.c) */
void pcm_stop(void);
#endif
