;; actors_a.s - the play loop's walk over the actor list (bank 1, next to PlayLoop and DoActor).
;; C calling convention of SDCC 4.4 (sdcccall(1)): the first 16 bit argument in HL.
        .module actors_a
        .area   _BANK1

        .globl  _run_actors, _obj, _player, _areabyplayer
        .globl  _actorat, _tics, _think_call, _action_call, _RemoveObj, b_RemoveObj, ___sdcc_bcall_ehl

;; offsets in objtype (def.h; wl_play.c checks them at compile time)
OBJ_TIC  = 1                            ; ticcount (s16)
OBJ_STATE = 4                           ; state (const statetype *, bank 0)
OBJ_FLAGS = 6
OBJ_TX   = 20                           ; tilex, then tiley
OBJ_AREA = 22                           ; areanumber
OBJ_NEXT = 45                           ; next
OBJ_ENC  = 49                           ; enc: the actor's actorat code
;; offsets in statetype
ST_TICTIME = 3
ST_THINK = 5
ST_ACTION = 6
ST_NEXT  = 7
FL_NEVERMARK = 4
FL_NONMARK = 128

        .area   _DATA
da_ob:  .ds     2                       ; the actor DoActor works on
        .area   _BANK1

;; void run_actors(void): for (obj = player; obj; obj = obj->next) DoActor(obj);
;; The first test of DoActor is "not active and its area is not connected to the player's: nothing to do", which
;; is true of most of the list most of the time: it is made here, without the call, and the list is followed with
;; one add (SDCC steps through the 45 bytes of the struct to the link, an inc hl at a time).  `obj` is still set
;; for every actor and the link is read from it after the call, as the C loop did.
_run_actors::
        ld      hl,(_player)
ra_loop:
        ld      (_obj),hl
        ld      a,h
        or      a,l
        ret     z                       ; (obj is 0 then, as at the end of the C loop)
        ld      a,(hl)                  ; active
        or      a,a
        jr      nz,ra_do
        ld      de,#OBJ_AREA
        add     hl,de
        ld      e,(hl)                  ; areanumber
        ld      d,#0
        ld      hl,#_areabyplayer
        add     hl,de
        ld      a,(hl)
        or      a,a
        ld      hl,(_obj)               ; (no flags change)
        jr      z,ra_next               ; inactive, not near the player: DoActor would return at once
ra_do:  call    DoActor                 ; hl = the actor
        ld      hl,(_obj)
ra_next:
        ld      de,#OBJ_NEXT
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a                     ; hl = obj->next
        jr      ra_loop

;; void DoActor(objtype *ob) (hl = ob): one step of an actor, as the C it replaces (wl_play.c, see git history):
;;   unmark its tile (unless FL_NONMARK / FL_NEVERMARK)
;;   ticcount 0: think.  Otherwise ticcount -= tics; while ticcount <= 0: the state's action, the next state
;;     (tictime 0: ticcount = 0 and stop), ticcount += tictime; then think
;;   the state gone after a think / an action, or no next state: RemoveObj, done
;;   mark its tile with enc (FL_NEVERMARK: never; FL_NONMARK: only when the tile is free)
;; Every field is read again after each call (the think and action routines change them).  The caller
;; (run_actors) has already made the "inactive and not near the player: nothing to do" test.
DoActor:
        ld      (da_ob),hl
        ld      de,#OBJ_FLAGS
        add     hl,de
        ld      a,(hl)
        and     a,#FL_NONMARK | FL_NEVERMARK
        jr      nz,1$
        ld      hl,(da_ob)
        call    da_at
        ld      (hl),#0                 ; actorat[tilex][tiley] = 0
1$:     ld      hl,(da_ob)
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; de = ticcount
        ld      a,d
        or      a,e
        jr      z,da_think
        ex      de,hl
        ld      bc,(_tics)
        or      a,a
        sbc     hl,bc
        ex      de,hl
        ld      (hl),d
        dec     hl
        ld      (hl),e                  ; ticcount -= tics
da_loop:                                ; while (ticcount <= 0)
        ld      hl,(da_ob)
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        bit     7,d
        jr      nz,2$
        ld      a,d
        or      a,e
        jr      nz,da_think
2$:     call    da_state
        ld      de,#ST_ACTION
        add     hl,de
        ld      a,(hl)
        or      a,a
        jr      z,3$
        ld      de,(da_ob)
        call    _action_call            ; action_call(action, ob)
        call    da_state
        ld      a,h
        or      a,l
        jr      z,da_remove
3$:     call    da_state                ; ob->state = ob->state->next
        ld      de,#ST_NEXT
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,(da_ob)
        ld      bc,#OBJ_STATE
        add     hl,bc
        ld      (hl),e
        inc     hl
        ld      (hl),d
        ld      a,d
        or      a,e
        jr      z,da_remove
        ex      de,hl
        inc     hl
        inc     hl
        inc     hl
        ld      c,(hl)
        inc     hl
        ld      b,(hl)                  ; bc = tictime
        ld      hl,(da_ob)
        inc     hl
        ld      a,b
        or      a,c
        jr      nz,4$
        ld      (hl),a                  ; tictime 0: ticcount = 0, think
        inc     hl
        ld      (hl),a
        jr      da_think
4$:     ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl
        add     hl,bc
        ex      de,hl
        ld      (hl),d
        dec     hl
        ld      (hl),e                  ; ticcount += tictime
        jr      da_loop
da_think:
        call    da_state
        ld      de,#ST_THINK
        add     hl,de
        ld      a,(hl)
        or      a,a
        jr      z,da_mark
        ld      de,(da_ob)
        call    _think_call             ; think_call(think, ob)
        call    da_state
        ld      a,h
        or      a,l
        jr      z,da_remove
da_mark:
        ld      hl,(da_ob)
        ld      de,#OBJ_FLAGS
        add     hl,de
        ld      c,(hl)
        bit     2,c                     ; FL_NEVERMARK
        ret     nz
        ld      hl,(da_ob)
        call    da_at
        bit     7,c                     ; FL_NONMARK: only a free tile
        jr      z,5$
        ld      a,(hl)
        or      a,a
        ret     nz
5$:     ex      de,hl
        ld      hl,(da_ob)
        ld      bc,#OBJ_ENC
        add     hl,bc
        ld      a,(hl)
        ld      (de),a                  ; actorat[tilex][tiley] = enc
        ret
da_remove:
        ld      hl,(da_ob)
        push    hl
        ld      e,#b_RemoveObj
        ld      hl,#_RemoveObj
        call    ___sdcc_bcall_ehl
        pop     af
        ret

;; hl = ob->state (uses de)
da_state:
        ld      hl,(da_ob)
        ld      de,#OBJ_STATE
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        ret

;; hl = ob -> hl = &actorat[ob->tilex][ob->tiley] (actorat[64][64]; uses a, de)
da_at:
        ld      de,#OBJ_TX
        add     hl,de
        ld      a,(hl)                  ; tilex
        inc     hl
        ld      e,(hl)                  ; tiley
        rrca
        rrca                            ; tilex * 64: high byte tilex >> 2, low byte (tilex & 3) << 6
        ld      d,a
        and     a,#0xC0
        or      a,e
        ld      e,a
        ld      a,d
        and     a,#0x3F
        ld      d,a
        ld      hl,#_actorat
        add     hl,de
        ret
