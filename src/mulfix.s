;; mulfix.s - SDCC 4.4.0's __mulsint2slong tests the sign in the LOW bytes
;; of its operands (bit 7,l / bit 7,e), so every operand whose low byte has
;; bit 7 set (e.g. 210, 0x1234 ... 0x12D2) gives a wrong 32 bit product.
;; This is the library code with the test on the high bytes.  Both routines
;; of the library module are defined here, so the library one is not linked.
;; HL * DE -> DEHL
        .module mulfix
        .globl  ___muluint2ulong
        .globl  ___mulsint2slong
        .area   _CODE

___mulsint2slong:
        ld      bc,#0                   ; c bit 0: negate the result
        bit     7,h
        jr      z,1$
        ld      a,b
        sub     a,l
        ld      l,a
        ld      a,b
        sbc     a,h
        ld      h,a
        inc     c
1$:     bit     7,d
        jr      z,2$
        ld      a,b
        sub     a,e
        ld      e,a
        ld      a,b
        sbc     a,d
        ld      d,a
        inc     c
2$:     push    bc
        call    ___muluint2ulong
        pop     bc
        bit     0,c
        ret     z
        ld      a,b
        sub     a,e
        ld      e,a
        ld      a,b
        sbc     a,d
        ld      d,a
        ld      a,b
        sbc     a,l
        ld      l,a
        ld      a,b
        sbc     a,h
        ld      h,a
        ret

;; 16x16 -> 32 unsigned: HL * DE -> DEHL
___muluint2ulong:
        ld      iy,#0
        ld      b,#16
3$:     add     iy,iy
        adc     hl,hl
        jr      nc,4$
        add     iy,de
        jr      nc,4$
        inc     hl
4$:     djnz    3$
        push    iy
        pop     de
        ret
