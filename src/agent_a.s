;; agent_a.s - the player's collision test (bank 2, next to TryMove in wl_agent.c).
;; C calling convention of SDCC 4.4 (sdcccall(1)): the first argument in HL, the result in A.
        .module agent_a
        .area   _DATA
tm_px:  .ds     4                       ; the object's x, y (copies)
tm_py:  .ds     4
tm_xl:  .ds     1                       ; the tiles it covers
tm_xh:  .ds     1
tm_yl:  .ds     1
tm_yh:  .ds     1
tm_nx:  .ds     1
tm_ny:  .ds     1
tm_chk: .ds     2
        .area   _BANK2
        .globl  _trymove_a, _actorat, _act_dec, _objlist

OB_FLAGS = 6
OB_X     = 12                           ; s32 x, then s32 y
PSH      = 0x58                         ; PLAYERSIZE >> 8 (its low byte is 0; wl_agent.c checks)

;; u8 trymove_a(objtype *ob): TryMove (wl_agent.c) for an object well inside the map: A = 1 (the move is free) or
;; 0, as the C; A = 2 when the object is near the edge of the map (a tile range that would need the C's clamps):
;; the C does it then.  The two tests are the C's:
;;   any tile it covers (x, y +- PLAYERSIZE) holds a wall / door (actorat nonzero and not an actor): blocked
;;   any tile one further holds a shootable actor (not the player) within MINACTORDIST in x and in y: blocked
;; (They have no side effects, so the order in which the tiles are tested doesn't matter.)
_trymove_a::
        ld      de,#OB_X
        add     hl,de
        ld      de,#tm_px
        ld      bc,#8
        ldir                            ; x, y
        ld      a,(tm_px + 3)
        ld      hl,#tm_py + 3
        or      a,(hl)
        jp      nz,tm_slow
        ld      hl,#tm_px + 1
        call    tm_range
        ld      (tm_xl),a
        ld      a,b
        ld      (tm_xh),a
        ld      hl,#tm_py + 1
        call    tm_range
        ld      (tm_yl),a
        ld      a,b
        ld      (tm_yh),a
        ld      a,(tm_xl)               ; 1 <= low, high <= 62: the C's clamps don't apply
        dec     a
        cp      a,#62
        jp      nc,tm_slow
        ld      a,(tm_yl)
        dec     a
        cp      a,#62
        jp      nc,tm_slow
        ld      a,(tm_xh)
        cp      a,#63
        jp      nc,tm_slow
        ld      a,(tm_yh)
        cp      a,#63
        jp      nc,tm_slow
        ld      a,(tm_xh)
        ld      hl,#tm_xl
        sub     a,(hl)
        inc     a
        ld      (tm_nx),a
        ld      a,(tm_yh)
        ld      hl,#tm_yl
        sub     a,(hl)
        inc     a
        ld      (tm_ny),a
        ;; the tiles it covers: walls, doors
        ld      a,(tm_xl)
        ld      e,a
        ld      a,(tm_yl)
        call    tm_ptr
        ld      a,(tm_nx)
        ld      c,a
1$:     push    hl
        ld      a,(tm_ny)
        ld      b,a
2$:     ld      a,(hl)
        or      a,a
        jr      z,3$
        and     a,#0x40
        jr      z,tm_wall               ; not an actor: blocked
3$:     inc     hl
        djnz    2$
        pop     hl
        ld      de,#64
        add     hl,de
        dec     c
        jr      nz,1$
        ;; one tile further: shootable actors
        ld      a,(tm_xl)
        dec     a
        ld      e,a
        ld      a,(tm_yl)
        dec     a
        call    tm_ptr
        ld      a,(tm_nx)
        add     a,#2
        ld      c,a
4$:     push    hl
        ld      a,(tm_ny)
        add     a,#2
        ld      b,a
5$:     ld      a,(hl)
        and     a,#0x40
        jr      z,7$
        push    bc
        push    hl
        ld      a,(hl)
        call    _act_dec                ; de = the actor
        ld      a,d
        or      a,e
        jr      z,6$
        ld      hl,#_objlist            ; (the player)
        or      a,a
        sbc     hl,de
        jr      z,6$
        ld      hl,#OB_FLAGS
        add     hl,de
        bit     0,(hl)                  ; FL_SHOOTABLE
        jr      z,6$
        ld      (tm_chk),de
        ld      hl,#OB_X
        add     hl,de
        ld      de,#tm_px
        call    tm_near
        jr      nz,6$
        ld      hl,(tm_chk)
        ld      de,#OB_X + 4
        add     hl,de
        ld      de,#tm_py
        call    tm_near
        jr      nz,6$
        pop     hl                      ; near in x and y: blocked
        pop     bc
        pop     hl
        xor     a,a
        ret
6$:     pop     hl
        pop     bc
7$:     inc     hl
        djnz    5$
        pop     hl
        ld      de,#64
        add     hl,de
        dec     c
        jr      nz,4$
        ld      a,#1
        ret
tm_wall:
        pop     hl
        xor     a,a
        ret
tm_slow:
        ld      a,#2
        ret

;; hl -> bytes 1, 2 of a coordinate: A = its tile - PLAYERSIZE, B = its tile + PLAYERSIZE ((v -+ 0x5800) >> 16)
tm_range:
        ld      c,(hl)
        inc     hl
        ld      b,(hl)
        ld      a,c
        cp      a,#PSH
        ld      a,b
        sbc     a,#0
        ld      d,a
        ld      a,c
        add     a,#PSH
        ld      a,b
        adc     a,#0
        ld      b,a
        ld      a,d
        ret

;; e = x, a = y -> hl = &actorat[x][y]
tm_ptr: ld      l,a
        ld      a,e
        rrca
        rrca
        ld      h,a
        and     a,#0xC0
        or      a,l
        ld      l,a
        ld      a,h
        and     a,#0x3F
        ld      h,a
        ld      de,#_actorat
        add     hl,de
        ret

;; hl -> the actor's coordinate, de -> ours (4 bytes each): Z when -MINACTORDIST <= ours - its <= MINACTORDIST
;; (MINACTORDIST = 0x10000: ours - its + 0x10000 is 0 .. 0x20000)
tm_near:
        ld      c,(hl)
        inc     hl
        ld      b,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; hl:bc = its
        push    hl
        ex      de,hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; hl:de = ours
        ex      de,hl
        or      a,a
        sbc     hl,bc
        ld      b,h
        ld      c,l                     ; bc = the difference's low word
        ex      de,hl
        pop     de
        sbc     hl,de                   ; hl = its high word
        inc     hl                      ; + 0x10000
        ld      a,h
        or      a,a
        ret     nz
        ld      a,l
        cp      a,#2
        jr      c,1$
        ret     nz
        ld      a,b
        or      a,c
        ret
1$:     xor     a,a
        ret
