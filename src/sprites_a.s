;; sprites_a.s - the sprite list of the 3-D view (bank 0): culling, projection, insertion.
;; C calling convention of SDCC 4.4 (sdcccall(1)): first argument A / HL, second L / DE, result A / DE.
;; The render data bank 2 (RD2: bearing table, shape tables) is in slot 1 while these routines run.
        .module sprites_a
        .include "gen/assets.inc"

VIEW_W     = VIEW_TW * 8                ; gen/assets.inc (render.c checks these)
MAXVISIBLE = 32
.if SPR_STAT_0 - 2
        .error  1
.endif

        .area   _DATA
;; the view, set by DrawScaleds: tile of the camera, its bearing in 1/256 turns
_vtx::  .ds     1
_vty::  .ds     1
_vang8:: .ds    1
;; the projection: per frame (xf_frame in render.c)
_xf_c14:: .ds   2                       ; viewcos >> 2 (2.14)
_xf_s14:: .ds   2                       ; viewsin >> 2
_xf_k1::  .ds   4                       ; tile centres: nx = ((dx * c14 - dy * s14) << 8) + k1
_xf_k2::  .ds   4                       ;               ny = ((dx * s14 + dy * c14) << 8) + k2
;; results of a projection
_xf_vx::  .ds   2                       ; view x of the sprite's centre
_xf_sh::  .ds   1                       ; half height in pixels
_xf_depth:: .ds 4                       ; nx >> 6 (xf_obj only)
xf_gx8:   .ds   2
xf_gy8:   .ds   2
xf_nx:    .ds   4
xf_ny:    .ds   4
xf_t1:    .ds   3
xf_dx:    .ds   1
xf_dy:    .ds   1
xf_n8:    .ds   2
xf_neg:   .ds   1
xf_wd:    .ds   1                       ; nonzero: xf_post also makes xf_depth
as_w:     .ds   2
as_x0:    .ds   2
ds_s0:    .ds   3
ds_type:  .ds   1
ds_tx:    .ds   1
ds_cnt:   .ds   1
ds_ptr:   .ds   2
as_shape: .ds   1
mk_neg:   .ds   1
;; the statics near the camera: pointers to the records within 14 tiles of the camera's tile, rebuilt when that tile
;; changes or the list of statics does (sc_valid = 0)
SC_CAP = 160
_col_ns::  .ds  28                      ; the sprites that reach each tile column: how many, and which (indexes into
_col_lst:: .ds  28 * 32                 ; vissprites, far to near), made by render_draw
rs_n:     .ds   1
rs_c:     .ds   1
rs_px:    .ds   1
rs_lst:   .ds   2
rs_v:     .ds   2
rs_ri:    .ds   1
rs_pxr:   .ds   1
rs_x:     .ds   1
rs_s:     .ds   1
rs_sbank: .ds   1
rs_saddr: .ds   2
rs_stepl: .ds   1
rs_steph: .ds   1
_sc_valid:: .ds 1
sc_vtx:   .ds   1
sc_vty:   .ds   1
sc_n:     .ds   1
sc_list:  .ds   SC_CAP * 2

        .area   _CODE

;; u8 in_view(u8 tx, u8 ty): A = tx, L = ty -> A = 1 when a sprite on that tile may be seen: within 14 tiles
;; each way, and either next to the camera or inside the viewing cone, widened by the sprite's size (55 / m)
;; for the nearer tiles.  Bearing table RD2_ATAN: row (dy + 14) * 32, column dx + 14.
        .globl  _in_view
_in_view::
        ld      c,l                     ; c = ty
        ld      hl,#_vtx
        sub     a,(hl)
        ld      b,a                     ; b = dx (signed)
        ld      a,c
        ld      hl,#_vty
        sub     a,(hl)
        ld      c,a                     ; c = dy (signed)
        ld      a,b                     ; d = |dx|
        or      a,a
        jp      p,1$
        neg
1$:     ld      d,a
        cp      a,#15
        jr      nc,iv_no
        ld      a,c                     ; e = |dy|
        or      a,a
        jp      p,2$
        neg
2$:     ld      e,a
        cp      a,#15
        jr      nc,iv_no
        ld      a,d
        cp      a,#2
        jr      nc,3$
        ld      a,e
        cp      a,#2
        jr      c,iv_yes                ; |dx| <= 1 and |dy| <= 1
3$:     ld      a,c                     ; hl = RD2_ATAN + (dy + 14) * 32 + dx + 14
        add     a,#14
        rrca
        rrca
        rrca
        ld      l,a
        and     a,#0x03
        or      a,#>RD2_ATAN
        ld      h,a
        ld      a,l
        and     a,#0xE0
        ld      l,a
        ld      a,b
        add     a,#14
        or      a,l
        ld      l,a
        ld      a,(hl)                  ; bearing of the tile
        ld      hl,#_vang8
        sub     a,(hl)
        cp      a,#129
        jr      c,4$
        neg                             ; |difference|
4$:     ld      c,a                     ; c = difference
        ld      a,d                     ; m = max(|dx|, |dy|)
        cp      a,e
        jr      nc,5$
        ld      a,e
5$:     ld      e,a
        ld      d,#0
        ld      hl,#iv_thr
        add     hl,de
        ld      a,(hl)                  ; PROJ_FOV8 + 2 + 55 / m
        cp      a,c
        jr      c,iv_no                 ; the limit is below the difference
iv_yes: ld      a,#1
        ret
iv_no:  xor     a,a
        ret
iv_thr: .db     0, 0
        .db     PROJ_FOV8 + 2 + 55 / 2
        .db     PROJ_FOV8 + 2 + 55 / 3
        .db     PROJ_FOV8 + 2 + 55 / 4
        .db     PROJ_FOV8 + 2 + 55 / 5
        .db     PROJ_FOV8 + 2 + 55 / 6
        .db     PROJ_FOV8 + 2 + 55 / 7
        .db     PROJ_FOV8 + 2 + 55 / 8
        .db     PROJ_FOV8 + 2 + 55 / 9
        .db     PROJ_FOV8 + 2 + 55 / 10
        .db     PROJ_FOV8 + 2 + 55 / 11
        .db     PROJ_FOV8 + 2 + 55 / 12
        .db     PROJ_FOV8 + 2 + 55 / 13
        .db     PROJ_FOV8 + 2 + 55 / 14

;; ------------------------------------------------------------------------------------------ projection
;; The view transform of the original: with gx8, gy8 = the object's offset from the camera in 1/256 tiles,
;;   nx = gx8 * c14 - gy8 * s14 - 0x100000          (depth, 10.22 fixed point)
;;   ny = gx8 * s14 + gy8 * c14                     (sideways)
;; Not seen when nx < 0x160000 (too close) or beyond the scale-0 distance; the half height is
;; KV / (nx >> 14) limited to MAXS; the centre column 112 +- |ny >> 14| * KH / (nx >> 14), not seen when that
;; is 512 or more (all as the C version that these routines replace, see tools/ and src/dbg.c).
        .globl  _smul16, _viewx, _viewy

KV14_HI = PROJ_KV_HALF_256 >> 2                         ; (PROJ_KV_HALF_256 << 14) >> 16
KV14_LO = (PROJ_KV_HALF_256 & 3) << 14
.if PROJ_KH - KHMUL_KH
        .error  1                                       ; xf_post multiplies by KHMUL_KH
.endif

;; div9: hl = remainder start (< de), a:c = the dividend's low 9 bits << 7, de = divisor (< 32768): the 9 bit
;; quotient in hl, de kept.  (n / d with n = start * 512 + low 9 bits, when the quotient is below 512)
div9:   ld      b,#9
1$:     sla     c
        rla
        adc     hl,hl
        sbc     hl,de
        jr      nc,2$
        add     hl,de
        jr      3$
2$:     inc     c
3$:     djnz    1$
        and     a,#1
        ld      h,a
        ld      l,c
        ret

;; xf_post: nx, ny (xf_nx, xf_ny) -> A = 1 and xf_vx, xf_sh (and xf_depth when xf_wd is set), or A = 0
xf_post:
        ld      hl,(xf_nx + 2)          ; nx >> 16
        bit     7,h
        jp      nz,xp_no                ; behind the camera
        ld      a,h
        or      a,a
        jr      nz,1$
        ld      a,l
        cp      a,#0x16
        jp      c,xp_no                 ; nx < 0x160000: too close
1$:     ld      de,#KV14_HI             ; nx >= KV << 14: scale 0
        or      a,a
        sbc     hl,de
        jr      c,2$
        jp      nz,xp_no
        ld      hl,(xf_nx)
        ld      de,#KV14_LO
        or      a,a
        sbc     hl,de
        jp      nc,xp_no
2$:     ld      hl,(xf_nx + 2)          ; nx8 = nx >> 14
        add     hl,hl
        add     hl,hl
        ld      a,(xf_nx + 1)
        rlca
        rlca
        and     a,#3
        or      a,l
        ld      l,a
        ld      (xf_n8),hl
        ex      de,hl                   ; de = nx8 (>= 88)
        ld      hl,#PROJ_KV_HALF_256 / MAXS
        or      a,a
        sbc     hl,de                   ; carry when nx8 > KV / MAXS
        ld      a,#MAXS                 ; (nearer: the limit)
        jr      nc,3$
        ld      hl,#PROJ_KV_HALF_256 >> 9
        ld      a,#>((PROJ_KV_HALF_256 & 511) << 7)
        ld      c,#<((PROJ_KV_HALF_256 & 511) << 7)
        call    div9                    ; hl = KV / nx8
        ld      a,l
3$:     ld      (_xf_sh),a
        ld      hl,(xf_ny + 2)          ; q = (s16)(ny >> 14)
        add     hl,hl
        add     hl,hl
        ld      a,(xf_ny + 1)
        rlca
        rlca
        and     a,#3
        or      a,l
        ld      l,a
        ld      b,#0                    ; b = 1 when q < 0
        bit     7,h
        jr      z,4$
        inc     b
        xor     a,a                     ; hl = -q
        sub     a,l
        ld      l,a
        ld      a,#0
        sbc     a,h
        ld      h,a
4$:     ld      a,b
        ld      (xf_neg),a
        ex      de,hl                   ; de = |q|
        .include "gen/khmul.inc"                ; num = |q| * KH, a:h:l
        srl     a                       ; bc = num >> 9
        ld      b,a
        ld      a,h
        rra
        ld      c,a                     ; (carry = bit 8 of num)
        ld      a,l
        rra                             ; a = bit 8 << 7 | l >> 1
        ld      l,#0
        rr      l                       ; l = (l & 1) << 7: the dividend's low 9 bits << 7 = a:l
        ld      (xf_t1),a
        ld      a,l
        ld      (xf_t1 + 1),a
        ld      de,(xf_n8)
        ld      h,b
        ld      l,c
        or      a,a
        sbc     hl,de
        jp      nc,xp_no                ; num >> 9 >= nx8: too far to the side
        ld      h,b
        ld      l,c
        ld      a,(xf_t1 + 1)
        ld      c,a
        ld      a,(xf_t1)
        call    div9                    ; hl = |q| * KH / nx8
        ex      de,hl
        ld      hl,#VIEW_W / 2
        ld      a,(xf_neg)
        or      a,a
        jr      z,5$
        sbc     hl,de
        jr      6$
5$:     add     hl,de
6$:     ld      (_xf_vx),hl
        ld      a,(xf_wd)
        or      a,a
        jr      z,7$
        ld      hl,(xf_nx + 2)          ; depth = nx >> 6 = (nx << 2) >> 8
        ld      de,(xf_nx)
        sla     e
        rl      d
        rl      l
        rl      h
        sla     e
        rl      d
        rl      l
        rl      h
        ld      a,d
        ld      (_xf_depth),a
        ld      a,l
        ld      (_xf_depth + 1),a
        ld      a,h
        ld      (_xf_depth + 2),a
        xor     a,a
        ld      (_xf_depth + 3),a
7$:     ld      a,#1
        ret
xp_no:  xor     a,a
        ret

;; mulk: a:hl = a * de, a a small signed number (|a| <= 15), de signed 16 bit
mulk:   ld      c,a
        xor     a,a
        bit     7,c
        jr      z,1$
        ld      a,c
        neg
        ld      c,a                     ; c = |a|
        ld      a,#1
1$:     ld      (mk_neg),a
        ld      b,#0
        bit     7,d
        jr      z,2$
        dec     b                       ; b = sign extension of de
2$:     ld      hl,#0
        xor     a,a
        bit     3,c
        jr      z,3$
        add     hl,de
        adc     a,b
3$:     add     hl,hl
        rla
        bit     2,c
        jr      z,4$
        add     hl,de
        adc     a,b
4$:     add     hl,hl
        rla
        bit     1,c
        jr      z,5$
        add     hl,de
        adc     a,b
5$:     add     hl,hl
        rla
        bit     0,c
        jr      z,6$
        add     hl,de
        adc     a,b
6$:     ld      c,a
        ld      a,(mk_neg)
        or      a,a
        ld      a,c
        ret     z
        ld      c,a                     ; negate
        xor     a,a
        sub     a,l
        ld      l,a
        ld      a,#0
        sbc     a,h
        ld      h,a
        ld      a,#0
        sbc     a,c
        ret

;; xf_dxy: b = dx, c = dy (the tile's offset from the camera's tile, |.| <= 14): the projection of the tile
;; centre from the per frame constants k1, k2 (no 16 bit products: dx, dy are small) -> A = 1 / 0, no xf_depth
xf_dxy: ld      a,b
        ld      (xf_dx),a
        ld      a,c
        ld      (xf_dy),a
        xor     a,a
        ld      (xf_wd),a
        ld      a,(xf_dx)               ; nx = ((dx * c14 - dy * s14) << 8) + k1
        ld      de,(_xf_c14)
        call    mulk
        ld      (xf_t1),hl
        ld      (xf_t1 + 2),a
        ld      a,(xf_dy)
        ld      de,(_xf_s14)
        call    mulk
        ld      c,a
        ex      de,hl
        ld      hl,(xf_t1)
        or      a,a
        sbc     hl,de
        ld      a,(xf_t1 + 2)
        sbc     a,c                     ; a:hl = dx * c14 - dy * s14
        ld      c,a
        ld      a,h
        ld      h,l
        ld      l,#0
        ld      de,(_xf_k1)
        add     hl,de
        ld      (xf_nx),hl
        ld      h,c
        ld      l,a
        ld      de,(_xf_k1 + 2)
        adc     hl,de
        ld      (xf_nx + 2),hl
        ld      a,(xf_dx)               ; ny = ((dx * s14 + dy * c14) << 8) + k2
        ld      de,(_xf_s14)
        call    mulk
        ld      (xf_t1),hl
        ld      (xf_t1 + 2),a
        ld      a,(xf_dy)
        ld      de,(_xf_c14)
        call    mulk
        ld      c,a
        ex      de,hl
        ld      hl,(xf_t1)
        add     hl,de
        ld      a,(xf_t1 + 2)
        adc     a,c
        ld      c,a
        ld      a,h
        ld      h,l
        ld      l,#0
        ld      de,(_xf_k2)
        add     hl,de
        ld      (xf_ny),hl
        ld      h,c
        ld      l,a
        ld      de,(_xf_k2 + 2)
        adc     hl,de
        ld      (xf_ny + 2),hl
        jp      xf_post

;; u8 xf_tile(u8 tx, u8 ty): A = tx, L = ty -> A = 1 / 0: the projection of that tile's centre (xf_vx, xf_sh)
        .globl  _xf_tile
_xf_tile::
        ld      c,l
        ld      hl,#_vtx
        sub     a,(hl)
        ld      b,a
        ld      a,c
        ld      hl,#_vty
        sub     a,(hl)
        ld      c,a
        jp      xf_dxy

;; u8 xf_obj(const objtype *ob): HL = ob -> A = 1 and xf_vx, xf_sh, xf_depth, or A = 0.  objtype.x, .y at 12, 16
OB_X = 12
OB_Y = 16
        .globl  _xf_obj
_xf_obj::
        ld      b,h
        ld      c,l                     ; bc = ob
        ld      de,#OB_X
        add     hl,de
        ld      a,(hl)                  ; x, byte 0 (only its borrow counts)
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; de = x >> 8
        ld      hl,#_viewx
        sub     a,(hl)
        inc     hl
        ld      a,e
        sbc     a,(hl)
        ld      e,a
        inc     hl
        ld      a,d
        sbc     a,(hl)
        ld      d,a                     ; de = gx8
        ld      (xf_gx8),de
        ld      h,b
        ld      l,c
        ld      de,#OB_Y
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,#_viewy
        sub     a,(hl)
        inc     hl
        ld      a,e
        sbc     a,(hl)
        ld      e,a
        inc     hl
        ld      a,d
        sbc     a,(hl)
        ld      d,a                     ; de = gy8
        ld      (xf_gy8),de
        ld      hl,(xf_gx8)             ; nx = gx8 * c14 - gy8 * s14 - 0x100000
        ld      de,(_xf_c14)
        call    _smul16                 ; de = low word, hl = high word
        ld      (xf_nx),de
        ld      (xf_nx + 2),hl
        ld      hl,(xf_gy8)
        ld      de,(_xf_s14)
        call    _smul16
        ex      de,hl                   ; hl = low word, de = high word
        push    de
        ex      de,hl
        ld      hl,(xf_nx)
        or      a,a
        sbc     hl,de
        ld      (xf_nx),hl
        pop     de
        ld      hl,(xf_nx + 2)
        sbc     hl,de
        ld      de,#0x0010
        or      a,a
        sbc     hl,de
        ld      (xf_nx + 2),hl
        ld      hl,(xf_gx8)             ; ny = gx8 * s14 + gy8 * c14
        ld      de,(_xf_s14)
        call    _smul16
        ld      (xf_ny),de
        ld      (xf_ny + 2),hl
        ld      hl,(xf_gy8)
        ld      de,(_xf_c14)
        call    _smul16
        ex      de,hl
        push    de
        ex      de,hl
        ld      hl,(xf_ny)
        add     hl,de
        ld      (xf_ny),hl
        pop     de
        ld      hl,(xf_ny + 2)
        adc     hl,de
        ld      (xf_ny + 2),hl
        ld      a,#1
        ld      (xf_wd),a
        jp      xf_post

;; ------------------------------------------------------------------------------------------ the sprite list
;; void add_sprite(u8 sh): A = shape; the sprite of the last projection (xf_vx, xf_sh) into vissprites[],
;; sorted far (small half height) to near; sprites of equal height keep their order.  VisSprite: x0 (2),
;; width (2), s, shape.  The width is shape_width * sc * 5 / 6 / 32 = shape_width * sc * 5 / 192.
        .globl  _add_sprite, _shape_width, _vissprites, _numvis
_add_sprite::
        ld      (as_shape),a
        ld      e,a
        ld      d,#0
        ld      hl,#_shape_width
        add     hl,de
        ld      b,(hl)                  ; b = the shape's columns
        ld      a,b
        or      a,a
        ret     z
        ld      a,(_xf_sh)
        ld      e,a
        ld      d,#0
        ld      hl,#0
        ld      a,#8
1$:     add     hl,hl                   ; hl = columns * sc
        sla     b
        jr      nc,2$
        add     hl,de
2$:     dec     a
        jr      nz,1$
        ld      d,h                     ; hl = 5 * columns * sc (< 65536)
        ld      e,l
        add     hl,hl
        add     hl,hl
        add     hl,de
        ld      a,l                     ; q1 = hl >> 6 = (h << 2) | (l >> 6)
        rlca
        rlca
        and     a,#3
        ld      e,a
        ld      l,h
        ld      h,#0
        add     hl,hl
        add     hl,hl
        ld      d,#0
        add     hl,de                   ; hl = q1 (<= 550); w = q1 / 3 = 85 * (q1 >> 8) + ((q1 & 255) + (q1 >> 8)) * 171 >> 9
        ld      b,h                     ; b = q1 >> 8 (0 .. 2)
        ld      h,#0                    ; hl = q1 & 255
        ld      e,b
        add     hl,de                   ; (d is 0) hl = y = (q1 & 255) + (q1 >> 8) (<= 257)
        ld      d,h
        ld      e,l
        add     hl,hl                   ; y * 171 (10101011b), most significant bit first
        add     hl,hl
        add     hl,de
        add     hl,hl
        add     hl,hl
        add     hl,de
        add     hl,hl
        add     hl,hl
        add     hl,de
        add     hl,hl
        add     hl,de
        ld      a,h
        srl     a                       ; a = y * 171 >> 9
        ld      c,a
        ld      a,b                     ; 85 * b = 5 * b * 17
        add     a,a
        add     a,a
        add     a,b
        ld      b,a
        add     a,a
        add     a,a
        add     a,a
        add     a,a
        add     a,b
        add     a,c                     ; a = w
        ret     z
        ld      (as_w),a
        srl     a                       ; x0 = vx - w / 2
        ld      e,a
        ld      d,#0
        ld      hl,(_xf_vx)
        or      a,a
        sbc     hl,de
        ld      (as_x0),hl
        ld      de,#VIEW_W              ; x0 >= VIEW_W: right of the view
        or      a,a
        sbc     hl,de
        bit     7,h
        ret     z
        ld      hl,(as_x0)              ; x0 + w <= 0: left of it
        ld      a,(as_w)
        ld      e,a
        ld      d,#0
        add     hl,de
        bit     7,h
        ret     nz
        ld      a,h
        or      a,l
        ret     z
        ld      a,(_numvis)
        cp      a,#MAXVISIBLE
        jr      nz,as_room
        ld      a,(_vissprites + 4)     ; full: not nearer than the farthest, nothing; else it makes room
        ld      b,a
        ld      a,(_xf_sh)
        cp      a,b
        ret     z
        ret     c
        ld      hl,#_vissprites + 6
        ld      de,#_vissprites
        ld      bc,#(MAXVISIBLE - 1) * 6
        ldir
        ld      a,#MAXVISIBLE - 1
        ld      (_numvis),a
as_room:
        ld      a,(_xf_sh)
        ld      c,a                     ; c = sc
        ld      a,(_numvis)
        ld      b,a                     ; b = sprites left to look at
        ld      hl,#_vissprites + 4
        or      a,a
        jr      z,as_found
        ld      de,#6
as_scan:
        ld      a,(hl)                  ; this sprite's half height
        cp      a,c
        jr      z,3$
        jr      nc,as_found             ; higher than the new one: the new one goes before it
3$:     add     hl,de
        djnz    as_scan
as_found:                               ; hl = &s of sprite j, b = numvis - j
        ld      de,#-4
        add     hl,de                   ; hl = &vissprites[j]
        push    hl
        ld      a,b
        or      a,a
        jr      z,as_put
        ld      c,a                     ; bc = 6 * b bytes to move up by one record
        add     a,a
        add     a,c
        add     a,a
        ld      c,a
        ld      b,#0
        add     hl,bc                   ; hl = &vissprites[numvis]
        ld      d,h
        ld      e,l
        push    bc
        ld      bc,#5
        add     hl,bc
        ex      de,hl                   ; de = last byte of the destination
        dec     hl                      ; hl = last byte of the source
        pop     bc
        lddr
as_put: pop     hl
        ld      de,(as_x0)
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      a,(as_w)
        ld      (hl),a
        inc     hl
        ld      (hl),#0
        inc     hl
        ld      a,(_xf_sh)
        ld      (hl),a
        inc     hl
        ld      a,(as_shape)
        ld      (hl),a
        ld      hl,#_numvis
        inc     (hl)
        ret

;; ------------------------------------------------------------------------------------------ the statics
;; One record: tile x, tile y, type (3 bytes; type 0xFF: taken, 0xFE: the sentinel below).
        .globl  _ds_statics, _statobjlist, _laststatobj, _statinfo_pic

;; ds_cand: hl -> a record that is not taken, b = vty, c = vtx: add_sprite if the tile is in the cone and the
;; projection sees it.  Keeps b, c; clobbers the rest.
ds_cand:
        push    bc
        ld      a,(hl)
        ld      (ds_tx),a
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      a,(hl)
        ld      (ds_type),a
        ld      a,(ds_tx)
        ld      l,e                     ; in_view(tx, ty)
        push    de
        call    _in_view
        pop     de                      ; e = ty
        pop     bc
        or      a,a
        ret     z
        ld      a,(ds_tx)               ; dx, dy
        sub     a,c
        ld      d,a
        ld      a,e
        sub     a,b
        ld      e,a
        push    bc
        ld      b,d                     ; b = dx
        ld      c,e                     ; c = dy
        call    xf_dxy
        pop     bc
        or      a,a
        ret     z
        ld      a,(ds_type)
        ld      e,a
        ld      d,#0
        ld      hl,#_statinfo_pic
        add     hl,de
        ld      a,(hl)
        add     a,#SPR_STAT_0
        push    bc
        call    _add_sprite
        pop     bc
        ret

;; void ds_statics(void): add_sprite for every static object in view
_ds_statics::
        ld      a,(_vtx)
        ld      c,a
        ld      a,(_vty)
        ld      b,a
        ld      a,(_sc_valid)
        or      a,a
        jr      z,dc_build
        ld      a,(sc_vtx)
        cp      a,c
        jr      nz,dc_build
        ld      a,(sc_vty)
        cp      a,b
        jr      nz,dc_build
dc_run: ld      a,(sc_n)                ; the cached list
        or      a,a
        ret     z
        ld      (ds_cnt),a
        ld      hl,#sc_list
        ld      (ds_ptr),hl
1$:     ld      hl,(ds_ptr)
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        inc     hl
        ld      (ds_ptr),hl
        ex      de,hl                   ; hl -> record
        inc     hl
        inc     hl
        ld      a,(hl)                  ; type
        dec     hl
        dec     hl
        cp      a,#0xFF
        jr      z,2$                    ; taken since
        call    ds_cand
2$:     ld      hl,#ds_cnt
        dec     (hl)
        jr      nz,1$
        ret

dc_build:                               ; scan every record, keep the ones within 14 tiles (sentinel at the end)
        ld      a,c
        ld      (sc_vtx),a
        ld      a,b
        ld      (sc_vty),a
        call    sentinel_put
        ld      hl,#_statobjlist
        ld      de,#sc_list
        xor     a,a
        ld      (sc_n),a
db_loop:
        ld      a,(hl)                  ; tile x
        sub     a,c
        jp      p,1$
        neg
1$:     cp      a,#15
        jr      nc,db_rej3
        inc     hl
        ld      a,(hl)                  ; tile y
        sub     a,b
        jp      p,2$
        neg
2$:     cp      a,#15
        jr      nc,db_rej2
        inc     hl
        ld      a,(hl)                  ; type
        cp      a,#0xFF
        jr      z,db_next               ; taken
        cp      a,#0xFE
        jr      z,db_done               ; the sentinel
        dec     hl                      ; keep a pointer to the record
        dec     hl
        ld      a,(sc_n)
        cp      a,#SC_CAP
        jr      z,db_full
        inc     a
        ld      (sc_n),a
        ld      a,l
        ld      (de),a
        inc     de
        ld      a,h
        ld      (de),a
        inc     de
        inc     hl
        inc     hl
db_next: inc    hl
        jr      db_loop
db_rej3: inc    hl
db_rej2: inc    hl
        inc     hl
        jr      db_loop
db_done:
        call    sentinel_get
        ld      a,#1
        ld      (_sc_valid),a
        jp      dc_run
db_full:                                ; too many to keep: no cache, draw this frame straight from the list
        call    sentinel_get
        xor     a,a
        ld      (_sc_valid),a
        call    sentinel_put
        ld      hl,#_statobjlist
df_loop:
        ld      a,(hl)
        sub     a,c
        jp      p,1$
        neg
1$:     cp      a,#15
        jr      nc,df_rej3
        inc     hl
        ld      a,(hl)
        sub     a,b
        jp      p,2$
        neg
2$:     cp      a,#15
        jr      nc,df_rej2
        inc     hl
        ld      a,(hl)
        cp      a,#0xFF
        jr      z,df_next
        cp      a,#0xFE
        jr      z,df_done
        dec     hl
        dec     hl
        push    hl
        call    ds_cand
        pop     hl
        inc     hl
        inc     hl
df_next: inc    hl
        jr      df_loop
df_rej3: inc    hl
df_rej2: inc    hl
        inc     hl
        jr      df_loop
df_done:
        jp      sentinel_get

;; sentinel_put: the 3 bytes at the end of the list are kept in ds_s0..2 and replaced by (vtx, vty, 0xFE), the
;; record that stops the scans; sentinel_get puts them back.  Both keep bc and de.
sentinel_put:
        ld      hl,(_laststatobj)
        ld      a,(hl)
        ld      (ds_s0),a
        ld      a,(_vtx)
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        ld      (ds_s0 + 1),a
        ld      a,(_vty)
        ld      (hl),a
        inc     hl
        ld      a,(hl)
        ld      (ds_s0 + 2),a
        ld      (hl),#0xFE
        ret

sentinel_get:
        ld      hl,(_laststatobj)
        ld      a,(ds_s0)
        ld      (hl),a
        inc     hl
        ld      a,(ds_s0 + 1)
        ld      (hl),a
        inc     hl
        ld      a,(ds_s0 + 2)
        ld      (hl),a
        ret

;; ------------------------------------------------------------------------------------------ the actors
;; objtype *ds_nextobj(objtype *ob): HL = ob (or 0) -> DE = the first object of the list from ob on that has a shape and
;; is in view (0 at the end).  An object with a shape that is out of view gets FL_VISABLE cleared.
OB_STATE = 4
OB_FLAGS = 6
OB_TILEX = 20
OB_TILEY = 21
OB_NEXT  = 45
        .globl  _ds_nextobj
_ds_nextobj::
        ld      a,h
        or      a,l
        jr      z,dn_end
dn_loop:
        push    hl
        ld      de,#OB_STATE
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; de = the state
        inc     de                      ; -> state->shapenum
        ld      a,(de)
        inc     de
        ld      b,a
        ld      a,(de)
        or      a,b
        jr      z,dn_next               ; no shape
        pop     hl
        push    hl
        ld      de,#OB_TILEX
        add     hl,de
        ld      a,(hl)                  ; tile x
        inc     hl
        ld      l,(hl)                  ; tile y
        call    _in_view
        or      a,a
        jr      z,dn_out
        pop     de
        ret
dn_out: pop     hl
        push    hl
        ld      de,#OB_FLAGS
        add     hl,de
        ld      a,(hl)
        and     a,#0xF7                 ; ~FL_VISABLE
        ld      (hl),a
dn_next:
        pop     hl
        ld      de,#OB_NEXT
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl
        ld      a,h
        or      a,l
        jr      nz,dn_loop
dn_end: ld      de,#0
        ret

;; ------------------------------------------------------------------------------------------ sprite rays
;; void rd_sprites(void): the rays of the sprites over tile column rd_tc (called by rd_cols after both wall rays are
;; in colbuf): for each sprite of the column's list, far to near, each of its two rays that falls inside it and is not
;; behind the wall (ray_s[ray] <= its half height) is drawn by the compiled sprite scaler (spr_ray).
        .globl  _rd_sprites, _rd_tc, _ray_s, _colright, _spr_seg, _spr_s, _spr_ray, _s1_rdbank
_rd_sprites::
        ld      a,(_rd_tc)
        ld      e,a
        ld      d,#0
        ld      hl,#_col_ns
        add     hl,de
        ld      a,(hl)
        or      a,a
        ret     z
        ld      (rs_n),a
        ld      l,e
        ld      h,#0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl                   ; tc * 32
        ld      de,#_col_lst
        add     hl,de
        ld      (rs_lst),hl
        ld      a,(_rd_tc)
        add     a,a
        ld      (rs_c),a                ; the column's first ray
        add     a,a
        add     a,a
        ld      (rs_px),a               ; its first pixel
rs_loop:
        ld      hl,(rs_lst)
        ld      a,(hl)
        inc     hl
        ld      (rs_lst),hl
        ld      e,a
        add     a,a
        add     a,e
        add     a,a                     ; 6 * index
        ld      e,a
        ld      d,#0
        ld      hl,#_vissprites
        add     hl,de
        ld      (rs_v),hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; de = x0
        ld      a,(rs_px)
        add     a,#2                    ; left ray: pixel px + 2
        ld      (rs_pxr),a
        ld      l,a
        ld      h,#0
        or      a,a
        sbc     hl,de                   ; l = px + 2 - x0
        push    hl
        ld      a,(rs_c)
        ld      (rs_ri),a
        xor     a,a
        ld      (_colright),a
        call    rs_ray
        pop     hl
        ld      de,#4
        add     hl,de                   ; the right ray, 4 pixels on
        ld      a,(rs_pxr)
        add     a,#4
        ld      (rs_pxr),a
        ld      a,(rs_c)
        inc     a
        ld      (rs_ri),a
        ld      a,#1
        ld      (_colright),a
        call    rs_ray
        ld      hl,#rs_n
        dec     (hl)
        jr      nz,rs_loop
        ld      a,(_s1_rdbank)
        ld      (#0xFFFE),a
        ret

;; rs_ray: hl = the ray's offset into the sprite rs_v, rs_pxr its pixel, rs_ri its ray: draws it when inside and in front
rs_ray: ex      de,hl                   ; de = l
        ld      hl,(rs_v)
        inc     hl
        inc     hl
        ld      c,(hl)
        inc     hl
        ld      b,(hl)                  ; bc = width
        ex      de,hl                   ; hl = l
        or      a,a
        sbc     hl,bc
        ret     nc                      ; l >= width (or l < 0)
        ld      a,(rs_ri)
        ld      e,a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      b,(hl)                  ; the wall's half height here
        ld      hl,(rs_v)
        ld      de,#4
        add     hl,de
        ld      a,(hl)                  ; the sprite's half height
        ld      (rs_s),a
        ld      c,a
        inc     hl
        ld      a,(hl)                  ; shape
        ld      e,a
        ld      a,c
        cp      a,b
        ret     c                       ; behind the wall
        ld      d,#0
        ld      a,#RD2_BANK             ; slot 1: the shape and step tables
        ld      (#0xFFFE),a
        ld      hl,#RD2_SHAPE_BANK
        add     hl,de
        ld      a,(hl)
        ld      (rs_sbank),a
        ld      hl,#RD2_SHAPE_ADDR
        add     hl,de
        add     hl,de
        ld      a,(hl)
        ld      (rs_saddr),a
        inc     hl
        ld      a,(hl)
        ld      (rs_saddr + 1),a
        ld      a,(rs_s)
        ld      e,a
        ld      d,#0
        ld      hl,#RD2_STEPINV
        add     hl,de
        add     hl,de
        ld      a,(hl)
        ld      (rs_stepl),a
        inc     hl
        ld      a,(hl)
        ld      (rs_steph),a
        ld      a,(_s1_rdbank)          ; slot 1: the square tables
        ld      (#0xFFFE),a
        ld      hl,(rs_v)               ; x = pixel - x0 (0 .. width - 1)
        ld      a,(rs_pxr)
        sub     a,(hl)
        ld      (rs_x),a
        ld      b,a
        ld      a,(rs_steph)
        ld      c,a
        call    mul8                    ; de = x * step high byte
        push    de
        ld      a,(rs_x)
        ld      b,a
        ld      a,(rs_stepl)
        ld      c,a
        call    mul8
        ld      l,d                     ; srccol = x * steph + (x * stepl >> 8) = x * step >> 8
        ld      h,#0
        pop     de
        add     hl,de
        ld      a,(rs_sbank)            ; slot 2: the shape
        ld      (#0xFFFF),a
        ld      de,(rs_saddr)
        ld      a,(de)
        ld      c,a                     ; c = the shape's source columns
        ld      a,h
        or      a,a
        jr      nz,1$
        ld      a,l
        cp      a,c
        jr      c,2$
1$:     ld      a,c                     ; srccol = columns - 1
        dec     a
        ld      l,a
        ld      h,#0
2$:     add     hl,hl
        inc     hl
        add     hl,de                   ; the column's offset: shape[1 + 2 * srccol]
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        add     hl,de
        ld      (_spr_seg),hl
        ld      a,(rs_s)
        ld      (_spr_s),a
        call    _spr_ray
        ld      a,(_s1_rdbank)
        ld      (#0xFFFE),a
        ret
