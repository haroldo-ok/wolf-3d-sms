;; fast.s - hand written Z80 helpers of the game logic (bank 0).  C calling convention of SDCC 4.4
;; (sdcccall(1)): the first 8 bit argument in A, a 16 bit one in HL, the second 16 bit one in DE;
;; the result in A (8 bit) or DE (16 bit).  Only AF, BC, DE, HL are used.
        .module fast
        .include "gen/assets.inc"
        .area   _CODE

;; objtype *act_dec(u8 v): the actor of an actorat byte (0x40 | id, id bit 6 in bit 7), 0 when v is
;; not an actor.  &objlist[id] = CR_OBJLIST + id * CR_OBJSIZE (50 = 32 + 16 + 2, checked by level.c)
        .globl  _act_dec
_act_dec::
        bit     6,a
        jr      nz,1$
        ld      de,#0
        ret
1$:     ld      c,a
        and     a,#0x3F
        ld      l,a
        ld      a,c
        and     a,#0x80
        rrca                            ; bit 7 -> bit 6
        or      a,l
        ld      l,a
        ld      h,#0                    ; hl = id
        add     hl,hl                   ; 2 id
        ld      d,h
        ld      e,l                     ; de = 2 id
        add     hl,hl
        add     hl,hl
        add     hl,hl                   ; 16 id
        ld      b,h
        ld      c,l
        add     hl,hl                   ; 32 id
        add     hl,bc                   ; 48 id
        add     hl,de                   ; 50 id
        ld      de,#CR_OBJLIST
        add     hl,de
        ex      de,hl
        ret

;; s32 mul_tics(u8 t, u16 v): A = t, DE = v -> DEHL (DE low word, HL high word) = t * v.  The speed of an
;; actor times the length of the step (the 32 bit library multiply costs 3300 cycles, this ~450)
        .globl  _mul_tics
_mul_tics::
        ld      c,a                     ; multiplier, most significant bit first
        ld      hl,#0
        ld      b,#0                    ; b:hl = product
        ld      a,#8
1$:     add     hl,hl
        rl      b
        rlc     c
        jr      nc,2$
        add     hl,de
        jr      nc,2$
        inc     b
2$:     dec     a
        jr      nz,1$
        ex      de,hl                   ; de = low word
        ld      l,b
        ld      h,#0                    ; hl = high word
        ret
