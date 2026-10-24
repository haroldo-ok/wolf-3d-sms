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

#ifdef DEBUG_DET
/* the deterministic test build has no sound: nothing in the interrupt that depends on timing */
void pcm_stop(void) {}
void snd_frame(void) { snd_ticks = 0; }
#else
static void silence(Voice *v)
{
    v->vol[0] = v->vol[1] = v->vol[2] = v->vol[3] = 15;
}

static void step(Voice *v)
{
    u8 b, ch;
    u16 off;
    if (!v->on || --v->wait)
        return;
    MapCtrl = 0;
    MapSlot2 = v->bank;
    for (;;)
    {
        b = *v->p++;
        if (!b)
        {
            off = v->p[0] | (v->p[1] << 8);
            if (!off)
            {
                v->on = 0;
                silence(v);
                return;
            }
            v->p = v->p - 1 - off;
            continue;
        }
        if (!(b & 0x80))
        {
            v->wait = b;
            return;
        }
        ch = (b >> 5) & 3;
        v->vol[ch] = b & 15;
        if (b & 0x10)
        {
            if (ch == 3)
                v->nctl = *v->p++;
            else
            {
                v->per[ch] = v->p[0] | (v->p[1] << 8);
                v->p += 2;
            }
        }
    }
}

static void mix(void)
{
    const Voice *s;
    u8 ch, v;
    u16 p;
    for (ch = 0; ch < 4; ch++)
    {
        if (pcm_on && (ch == 1 || ch == 2))
            continue;
        s = (sfx.on && ch >= 2) ? &sfx : &mus;
        if (ch < 3)
        {
            p = s->per[ch];
            if (p != cur_per[ch])
            {
                cur_per[ch] = p;
                PSGPort = 0x80 | (ch << 5) | (p & 15);
                PSGPort = (u8)(p >> 4) & 0x3F;
            }
        }
        else if (s->nctl != cur_nctl)
        {
            cur_nctl = s->nctl;
            PSGPort = 0xE0 | (cur_nctl & 7);
        }
        v = s->vol[ch];
        if (v != cur_vol[ch])
        {
            cur_vol[ch] = v;
            PSGPort = 0x90 | (ch << 5) | v;
        }
    }
}

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
        step(&mus);
        step(&sfx);
    }
    snd_ticks = 0;
    if (pcm_on == 1)
        pcm_refill();
    mix();
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
