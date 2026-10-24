/* input.c - the pad (banked) */
#include "game/def.h"

u8 pad_now, pad_last;

/* sample the pad (done once per frame by the play loop) */
void IN_Update(void) B
{
    pad_last = pad_now;
    pad_now = pad_read();
}

void IN_ClearKeys(void) B
{
    IN_Update();
    while (pad_now & (PAD_1 | PAD_2 | PAD_PAUSE))
    {
        wait_vblank();
        IN_Update();
    }
    pad_last = pad_now;
}

/* a button went down since the last call */
u8 IN_CheckAck(void) B
{
    IN_Update();
    return (pad_new & (PAD_1 | PAD_2 | PAD_PAUSE)) != 0;
}

void IN_Ack(void) B
{
    while (!IN_CheckAck())
        wait_vblank();
}

/* wait delay tics (1/70 s) or a button; TRUE if pressed */
u8 IN_UserInput(u32 delay) B
{
    u32 lasttime = get_timecount();
    do
    {
        wait_vblank();
        if (IN_CheckAck())
            return 1;
    } while (get_timecount() - lasttime < delay);
    return 0;
}
