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
;; actor times the length of the step (the 32 bit library multiply costs 3300 cycles, this ~250)
        .globl  _mul_tics
_mul_tics::
        ld      hl,#0                   ; a:hl = a * de, unrolled: each step shifts a:hl left, the next bit of the
        add     hl,hl                   ; multiplier (a) comes out in carry
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
        ex      de,hl                   ; de = low word
        ld      l,a
        ld      h,#0                    ; hl = high word
        ret

;; ___sdcc_bcall_ehl: the trampoline of every banked call (E = the function's bank, HL = its address), replacing
;; the one of SDCC's runtime.  The same stack layout (one byte, the caller's bank, between the return address
;; of the call and that of the function) and the same registers (the result in A / DE / HL survives, BC does
;; not), but the mapper register (slot 1 at 0xFFFE) is read and written in line, not through get_bank and
;; set_bank calls: ~130 cycles instead of ~210 for every call into the game's banks.
        .globl  ___sdcc_bcall_ehl
___sdcc_bcall_ehl::
        ld      a,(#0xfffe)             ; the caller's bank
        push    af
        inc     sp                      ; (only the bank byte stays)
        ld      a,e
        ld      (#0xfffe),a             ; the function's bank
        call    bc_jphl
        dec     sp
        pop     bc                      ; b = the caller's bank
        push    af                      ; the result in A
        ld      a,b
        ld      (#0xfffe),a
        pop     af
        ret
bc_jphl:
        jp      (hl)

;; u8 tpath_fast(objtype *ob) (hl = ob): the common case of T_Path (wl_act2.c), done here in bank 0 without the
;; banked calls to T_Path and MoveObj; A = 1: done, exactly as T_Path would have; A = 0: nothing was changed, T_Path
;; must run.  The common case: SightPlayer would return at once (SIGHT_SKIP: not in attack mode, not counting down
;; a reaction, the area not connected to the player's - so MoveObj's check of the player's position is skipped
;; too), a direction, no door to wait for, and the move ends before the next tile (move < distance):
;;   move = mul_tics(tics, speed); x, y += move by the direction; distance -= move
;; (move 0: T_Path does nothing.)  wl_play.c checks the offsets at compile time.
TP_FLAGS = 6
TP_DIST  = 7                            ; s32 distance
TP_DIR   = 11
TP_X     = 12                           ; s32 x, then s32 y
TP_AREA  = 22
TP_SPEED = 35                           ; s32 speed
TP_TEMP2 = 41
        .area   _DATA
tp_ob:  .ds     2
tp_mv:  .ds     4
tp_nd:  .ds     4
        .area   _CODE
        .globl  _tpath_fast, _areabyplayer, _tics
_tpath_fast::
        ld      (tp_ob),hl
        ld      de,#TP_FLAGS
        add     hl,de
        bit     4,(hl)                  ; FL_ATTACKMODE
        jp      nz,tp_no
        ld      hl,(tp_ob)
        ld      de,#TP_TEMP2
        add     hl,de
        ld      a,(hl)
        inc     hl
        or      a,(hl)
        jp      nz,tp_no                ; reacting
        ld      hl,(tp_ob)
        ld      de,#TP_AREA
        add     hl,de
        ld      e,(hl)
        ld      d,#0
        ld      hl,#_areabyplayer
        add     hl,de
        ld      a,(hl)
        or      a,a
        jp      nz,tp_no                ; near the player
        ld      hl,(tp_ob)
        ld      de,#TP_DIR
        add     hl,de
        ld      a,(hl)
        cp      a,#8
        jp      nc,tp_no                ; nodir
        ld      e,a
        ld      d,#0
        ld      hl,#tp_dirs
        add     hl,de
        ld      a,(hl)
        push    af                      ; what the direction moves
        ld      hl,(tp_ob)
        ld      de,#TP_SPEED
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      a,(_tics)
        call    _mul_tics               ; hl:de = move
        ld      a,d
        or      a,e
        or      a,l
        jr      z,tp_done               ; no move
        ld      (tp_mv),de
        ld      (tp_mv + 2),hl
        ld      hl,(tp_ob)
        ld      de,#TP_DIST
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; hl:de = distance
        bit     7,h
        jr      nz,tp_pop               ; waiting for a door
        ex      de,hl
        ld      bc,(tp_mv)
        or      a,a
        sbc     hl,bc
        ld      (tp_nd),hl
        ex      de,hl
        ld      bc,(tp_mv + 2)
        sbc     hl,bc
        jr      c,tp_pop                ; move > distance: the next tile
        ld      (tp_nd + 2),hl
        ld      a,h
        or      a,l
        or      a,d
        or      a,e
        jr      z,tp_pop                ; move == distance
        ;; commit: x, y, distance
        pop     af
        ld      c,a
        ld      hl,(tp_ob)
        ld      de,#TP_X
        add     hl,de
        bit     0,c
        call    nz,tp_addsub            ; x (bit 1: minus)
        ld      hl,(tp_ob)
        ld      de,#TP_X + 4
        add     hl,de
        srl     c
        srl     c
        bit     0,c
        call    nz,tp_addsub            ; y (bit 3: minus)
        ld      hl,(tp_ob)
        ld      de,#TP_DIST
        add     hl,de
        ld      de,(tp_nd)
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      de,(tp_nd + 2)
        ld      (hl),e
        inc     hl
        ld      (hl),d
        ld      a,#1
        ret
tp_done:
        pop     af
        ld      a,#1
        ret
tp_pop: pop     af
tp_no:  xor     a,a
        ret
;; (hl) += move, or -= move when bit 1 of c is set (c kept; move < 2^24: e, d, b = its bytes)
tp_addsub:
        ld      de,(tp_mv)
        ld      a,(tp_mv + 2)
        ld      b,a
        bit     1,c
        jr      nz,1$
        ld      a,(hl)
        add     a,e
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        adc     a,d
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        adc     a,b
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        adc     a,#0
        ld      (hl),a
        ret
1$:     ld      a,(hl)
        sub     a,e
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        sbc     a,d
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        sbc     a,b
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        sbc     a,#0
        ld      (hl),a
        ret
;; per direction (east, northeast, north, northwest, west, southwest, south, southeast): bit 0 x moves, bit 1 by
;; -move; bit 2 y moves, bit 3 by -move
tp_dirs: .db    1, 13, 12, 15, 3, 7, 4, 5
