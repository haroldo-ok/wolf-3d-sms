/* snd.c - the sound player (bank 0): SN76489 streams (tools/wl_audio.py) from the frame interrupt, and sampled
 * voices from the line interrupt.
 *
 * Music and effects are streams of events (see wl_audio.py) read through slot 2 from the interrupt: the interrupt
 * maps the stream's bank and gives slot 2 back as it found it (the mapper registers read back from their RAM mirror).  The music owns the three tone channels and the noise
 * channel, an effect takes channel 2 and the noise channel from it while it lasts.  A sampled voice takes channels
 * 1 and 2: one byte per sample, the two attenuations, written by the line interrupt (isr_vblank in hw.c) from a RAM
 * buffer that this code fills at every frame interrupt (the line interrupt never touches the mapper). */
#include "hw.h"
#include "snd.h"
#include <string.h>
#include <stddef.h>

u8 SoundOn = 1, MusicOn = 1, DigiOn = 1;      /* DigiOn: 0 none, 1 voices, 2 all */

Voice mus, sfx;
u16 cur_per[3];
u8 cur_vol[4], cur_nctl;
u8 snd_ticks;                           /* 70 Hz ticks to run at this frame interrupt (hw.c) */
u8 pcm_on;
const u8 *pcm_ptr;
u8 pcm_bank;
const u8 *pcm_src;
u16 pcm_left;                           /* frames */
u8 pcm_buf[192 + 16];                   /* 96 samples of two bytes and silence for a few more interrupts */

/* the offsets snd_a.s uses */
typedef char chk_voice[(offsetof(Voice, on) == 0 && offsetof(Voice, bank) == 1 && offsetof(Voice, p) == 2
                        && offsetof(Voice, wait) == 4 && offsetof(Voice, per) == 8 && offsetof(Voice, vol) == 14
                        && offsetof(Voice, nctl) == 18) ? 1 : -1];

#ifdef DEBUG_DET
/* the deterministic test build has no sound: nothing in the interrupt that depends on timing */
void pcm_stop(void) {}
void snd_frame(void) { snd_ticks = 0; }
#else
/* snd_step (one tick of a voice's stream) and snd_mix (what the PSG needs to be told): snd_a.s.  The C versions
 * they replace took 1000 and 2200 cycles a call, 70 times a second. */
void snd_step(Voice *v);
void snd_mix(void);

void pcm_stop(void)
{
    u8 i;
    pcm_on = 0;
    VDPCtrl = 0x06;                     /* line interrupt off (VDP register 0): in the interrupt or with it off,
                                         * so not vdp_reg(), which ends with ei */
    VDPCtrl = 0x80;
    for (i = 0; i < 3; i++)
        cur_per[i] = 0xFFFF;
    for (i = 0; i < 4; i++)
        cur_vol[i] = 0xFF;
    cur_nctl = 0xFF;
}

static void pcm_refill(void)
{
    MapCtrl = 0;
    MapSlot2 = pcm_bank;
    memcpy(pcm_buf, pcm_src, 192);
    pcm_ptr = pcm_buf;
    pcm_src += 192;
    if (pcm_src >= (const u8 *)(0x8000 + 85 * 192))
    {
        pcm_src = (const u8 *)0x8000;
        pcm_bank++;
    }
    if (!--pcm_left)
        pcm_on = 2;                     /* the last buffer is on its way: stop at the next frame */
}

/* the frame interrupt (hw.c): the 70 Hz ticks, the sampled voice */
void snd_frame(void)
{
    u8 k, ctrl = MapCtrl, bank2 = MapSlot2;       /* the mapper as it is now: the registers read back from the RAM mirror (the
                                                 * renderer maps slot 2 itself, slot2_cur does not know) */
    if (pcm_on == 2)
        pcm_stop();
    for (k = snd_ticks; k; k--)
    {
        snd_step(&mus);
        snd_step(&sfx);
    }
    snd_ticks = 0;
    if (pcm_on == 1)
        pcm_refill();
    snd_mix();
    MapSlot2 = bank2;
    MapCtrl = ctrl;
}

#endif

/* power on: the PSG silent (the voices are set by snd_ctl.c) */
void SD_Startup(void)
{
    PSGPort = 0x9F;
    PSGPort = 0xBF;
    PSGPort = 0xDF;
    PSGPort = 0xFF;
}
