/* mathz.c - bank 0: fixed point and integer helpers (Z80) */
#include "hw.h"
#include "mathz.h"

/* ------------------------------------------------------------- math */

/* FixedByFrac: a 16.16 number times a fraction; like the original only the
 * fraction's low 16 bits of magnitude count (sin 90 = 65535).
 * Two 16x16->32 multiplies (the library's shift-add routine). */
u32 __muluint2ulong(u16 a, u16 b);     /* SDCC library, 16x16 -> 32 */
fixed FixedByFrac(fixed a, fixed b)
{
    u8 neg = 0;
    u32 ua, r;
    u16 ub, hi, lo;
    if (a < 0) { ua = -a; neg = 1; } else ua = a;
    if (b < 0) { ub = (u16)(-b); neg ^= 1; } else ub = (u16)b;
    hi = ((u16 *)&ua)[1];
    lo = ((u16 *)&ua)[0];
    if (lo)
    {
        r = __muluint2ulong(lo, ub);
        lo = ((u16 *)&r)[1];
    }
    r = hi ? __muluint2ulong(hi, ub) + lo : lo;
    return neg ? -(fixed)r : (fixed)r;
}

/* n / d for n < d * 65536 (16 bit quotient), restoring division */
static u32 dv_n;
static u16 dv_d;
static u16 udiv_nd(void) __naked
{
    __asm
        ld hl,(_dv_n+2)
        ld de,(_dv_n)
        ld bc,(_dv_d)
        ld a,#16
00001$:
        ex de,hl
        add hl,hl
        ex de,hl
        adc hl,hl
        jr c,00003$
        or a,a
        sbc hl,bc
        jr nc,00002$
        add hl,bc
        jr 00004$
00003$:
        or a,a
        sbc hl,bc
00002$:
        inc e
00004$:
        dec a
        jr nz,00001$
        ret
    __endasm;
}

u16 udiv32_16(u32 n, u16 d)
{
    dv_n = n;
    dv_d = d;
    return udiv_nd();
}

/* xorshift PRNG (SGDK's random() stand in) */
static u16 rnd_state = 0xACE1;
u16 random16(void)
{
    u16 x = rnd_state;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    rnd_state = x;
    return x;
}

void random_seed(u16 s)
{
    rnd_state = s ? s : 0xACE1;
}

/* signed 16x16 -> 32 (HL * DE -> HLDE).  SDCC 4.4.0's __mulsint2slong
 * tests the sign in the LOW bytes (bit 7,l / bit 7,e) and gets every
 * operand with bit 7 of the low byte set wrong; this is it fixed. */
long smul16(int a, int b) __naked
{
    (void)a; (void)b;
    __asm
        ld bc,#0
        bit 7,h
        jr z,00001$
        ld a,b
        sub a,l
        ld l,a
        ld a,b
        sbc a,h
        ld h,a
        inc c
00001$:
        bit 7,d
        jr z,00002$
        ld a,b
        sub a,e
        ld e,a
        ld a,b
        sbc a,d
        ld d,a
        inc c
00002$:
        push bc
        call ___muluint2ulong
        pop bc
        bit 0,c
        ret z
        ld a,b
        sub a,e
        ld e,a
        ld a,b
        sbc a,d
        ld d,a
        ld a,b
        sbc a,l
        ld l,a
        ld a,b
        sbc a,h
        ld h,a
        ret
    __endasm;
}


/* sin_q[a]: sine of a degrees * 65536, saturated to 65535 (assets_tab.c) */
extern const u16 sin_q[91];
fixed fsin(s16 a)
{
    while (a >= 360) a -= 360;
    while (a < 0) a += 360;
    if (a <= 90) return sin_q[a];
    if (a <= 180) return sin_q[180 - a];
    if (a <= 270) return -(fixed)sin_q[a - 180];
    return -(fixed)sin_q[360 - a];
}

fixed fcos(s16 a)
{
    return fsin(a + 90);
}
