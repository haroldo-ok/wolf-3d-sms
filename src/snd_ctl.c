/* snd_ctl.c - the sound API of the game (port of ID_SD.C), on top of the player in snd.c (banked) */
#include "game/def.h"
#include "snd.h"

extern const u8 sfx_bank[NUM_SFX];
extern const u16 sfx_addr[NUM_SFX];
extern const u8 sfx_pri[NUM_SFX];
extern const u8 mus_bank[NUM_SONGS];
extern const u16 mus_addr[NUM_SONGS];
extern const u8 digi_of[88];
extern const u16 pcm_start[16];
extern const u16 pcm_frames[16];
extern const u8 digi_voice[16];

static void silence(Voice *v)
{
    v->vol[0] = v->vol[1] = v->vol[2] = v->vol[3] = 15;
}

/* the player (snd.c) shares these with the interrupt: changed with interrupts off */
void snd_init(void) B
{
    u8 i;
    __asm__("di");
    mus.on = sfx.on = 0;
    silence(&mus);
    silence(&sfx);
    for (i = 0; i < 3; i++)
        mus.per[i] = sfx.per[i] = 0;
    mus.nctl = sfx.nctl = 4;
    pcm_on = 0;
    for (i = 192; i < 208; i += 2)
    {
        pcm_buf[i] = 0xBF;              /* after the samples of a frame: silent */
        pcm_buf[i + 1] = 0xDF;
    }
    for (i = 0; i < 3; i++)
        cur_per[i] = 0xFFFF;
    for (i = 0; i < 4; i++)
        cur_vol[i] = 0xFF;
    cur_nctl = 0xFF;
    __asm__("ei");
}

static void snd_music(u8 bank, const u8 *p)
{
    __asm__("di");
    silence(&mus);
    mus.bank = bank;
    mus.p = p;
    mus.wait = 1;
    mus.on = p != 0;
    __asm__("ei");
}

static void snd_sfx(u8 bank, const u8 *p, u8 pri)
{
    __asm__("di");
    silence(&sfx);
    sfx.bank = bank;
    sfx.p = p;
    sfx.pri = pri;
    sfx.wait = 1;
    sfx.on = 1;
    __asm__("ei");
}

static void snd_pcm(u8 bank, u16 addr, u16 frames)
{
    __asm__("di");
    pcm_bank = bank;
    pcm_src = (const u8 *)addr;
    pcm_left = frames;
    pcm_on = 1;
    /* channels 1 and 2 at the highest frequency: the volume is the sample */
    PSGPort = 0xA1; PSGPort = 0x00;
    PSGPort = 0xC1; PSGPort = 0x00;
    PSGPort = 0xBF; PSGPort = 0xDF;
    pcm_ptr = pcm_buf + 192;            /* silence until the first frame interrupt has filled the buffer */
    vdp_reg(10, 1);
    vdp_reg(0, 0x16);                   /* line interrupt every second line */
    __asm__("ei");
}

static void snd_sfx_stop(void)
{
    __asm__("di");
    sfx.on = 0;
    silence(&sfx);
    __asm__("ei");
}

static void snd_pcm_stop(void)
{
    __asm__("di");
    if (pcm_on)
        pcm_stop();
    __asm__("ei");
}

#define snd_music_busy()    (mus.on)
#define snd_sfx_busy()      (sfx.on)
#define snd_sfx_pri()       (sfx.pri)
#define snd_pcm_busy()      (pcm_on)

static u16 cur_sound;
static u8 cur_song = 0xFF;

u8 SD_PlaySound(u16 sound) B
{
    u8 d;
    u16 st;
    if (sound >= NUM_SFX)
        return 0;
    d = digi_of[sound];
    if (DigiOn && d != 0xFF && (DigiOn == 2 || digi_voice[d]))      /* 1: the voices, 2: every digitised sound */
    {
        st = pcm_start[d];
        snd_pcm(PCM_BANK0 + st / PCM_PER_BANK, 0x8000 + (st % PCM_PER_BANK) * PCM_FRAME, pcm_frames[d]);
        cur_sound = sound;
        return 1;
    }
    if (!SoundOn)
        return 0;
    if (snd_sfx_busy() && sfx_pri[sound] < snd_sfx_pri())
        return 0;
    snd_sfx(sfx_bank[sound], (const u8 *)sfx_addr[sound], sfx_pri[sound]);
    cur_sound = sound;
    return 1;
}

void SD_StopSound(void) B
{
    snd_sfx_stop();
    snd_pcm_stop();
    cur_sound = 0;
}

u16 SD_SoundPlaying(void) B
{
    return (snd_sfx_busy() || snd_pcm_busy()) ? cur_sound : 0;
}

void SD_WaitSoundDone(void) B
{
    u16 t = 0;
    while (SD_SoundPlaying() && t++ < 500)
        wait_vblank();
}

void SD_StartMusic(u16 song) B
{
    if (!MusicOn || song >= NUM_SONGS)
        return;
    if (song == cur_song && snd_music_busy())
        return;
    cur_song = (u8)song;
    snd_music(mus_bank[song], (const u8 *)mus_addr[song]);
}

void SD_MusicOff(void) B
{
    cur_song = 0xFF;
    snd_music(0, 0);
}

void SD_FadeOutMusic(void) B
{
    SD_MusicOff();
}
