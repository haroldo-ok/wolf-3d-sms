/* plat0.c - bank 0: timer, ROM data access, fatal errors */
#include "game/def.h"
#include <string.h>

u32 get_timecount(void)
{
    u32 t;
    __asm__("di");
    t = time_count;
    __asm__("ei");
    return t;
}

void set_timecount(u32 t)
{
    __asm__("di");
    time_count = t;
    __asm__("ei");
}

/* copy from a data bank (addresses as packed for slot 2, 0x8000..) through
 * slot 1; dst may be anywhere but slot 1 */
void rom_read(u8 bank, u16 addr, void *dst, u16 len)
{
    u8 old = MapSlot1;
    MapSlot1 = bank;
    memcpy(dst, (const u8 *)(addr - 0x4000), len);
    MapSlot1 = old;
}

#ifdef DEBUG_MIRRORS
char dbg_quitbuf[48];       /* smstest: dumpstate, then read this symbol */
#endif
void Quit(const char *error)
{
#ifdef DEBUG_MIRRORS
    u8 i;
    for (i = 0; error && i < 47 && error[i]; i++)
        dbg_quitbuf[i] = error[i];
    dbg_quitbuf[i] = 0;
#endif
    vdp_display(0);
    cram_set(0, 0x03);
    cram_set(1, 0x3F);
    vdp_reg(7, 0xF0);
    vdp_display(1);
    for (;;)
        __asm__("halt");
}
