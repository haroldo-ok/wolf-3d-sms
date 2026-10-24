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

;; 16x16 -> 32 unsigned: HL * DE -> HL:DE (high word HL, low word DE)
;; Two unrolled 8 x 16 bit passes (the low, then the high byte of HL) instead of 16 turns of a loop on IY:
;; ~550 cycles instead of ~1000; 0 in the high byte of HL (common) skips the second pass.
;; In each step A:HL (24 bits) shifts left: the multiplier's next bit (from A) comes out in carry.
___muluint2ulong:
        ld      c,h                     ; c = the high byte of the multiplier
        ld      a,l
        ld      hl,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        ld      b,a
        ld      a,c
        or      a,a
        jr      nz,5$
        ex      de,hl                   ; high byte 0: A:HL = the product (24 bits)
        ld      l,b
        ld      h,a
        ret
5$:     push    hl                      ; b:hl = low byte * DE
        ld      hl,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        add     hl,hl
        rla
        jr      nc,.+5
        add     hl,de
        adc     a,#0
        pop     de                      ; a:hl = high byte * DE; result = b:de + (a:hl << 8)
        ld      c,a
        ld      a,d
        add     a,l
        ld      d,a
        ld      a,b
        adc     a,h
        ld      l,a
        ld      a,c
        adc     a,#0
        ld      h,a
        ret
