;; snd_a.s - the sound player's inner loops (bank 0), run from the frame interrupt 70 times a second:
;; snd_step(Voice *v) reads a voice's stream, snd_mix() writes the PSG what changed.  The C versions
;; (snd.c, see git history) took ~1000 / ~2200 cycles a call; these are the same logic, byte for byte
;; (the sequence of PSG writes is identical).  C calling convention of SDCC 4.4 (sdcccall(1)): the first
;; 16 bit argument in HL.  Only AF, BC, DE, HL are used: not the alternate set, which the renderer keeps
;; state in, nor IX / IY.
        .module snd_a
        .area   _CODE

        .globl  _snd_step, _snd_mix
        .globl  _mus, _sfx, _cur_per, _cur_vol, _cur_nctl, _pcm_on

;; Voice (snd.h; snd.c checks the offsets at compile time)
V_ON    = 0
V_BANK  = 1
V_P     = 2
V_WAIT  = 4
V_PER   = 8                             ; u16 per[3]
V_VOL   = 14                            ; u8 vol[4]
V_NCTL  = 18

PSG     = 0x7F
MAPCTRL = 0xFFFC
MAPSLOT2 = 0xFFFF

;; void snd_step(Voice *v): one 70 Hz tick of a voice.  Nothing happens while it is off or waiting; when the
;; wait is over the stream (in the voice's bank, slot 2) is read up to the next wait:
;;   0, lo, hi   jump back by lo:hi (0:0 = the end: the voice goes off, silent)
;;   1..127      wait that many ticks
;;   128..255    bits 5-6 the channel, 0-3 low bits its volume, bit 4: a period (2 bytes) / noise control
;;               (1 byte) follows
;; bc = v, hl = the stream pointer while it runs
_snd_step::
        ld      a,(hl)
        or      a,a
        ret     z                       ; off
        ld      de,#V_WAIT
        add     hl,de
        dec     (hl)
        ret     nz                      ; still waiting
        ld      de,#-V_WAIT
        add     hl,de                   ; hl = v
        ld      b,h
        ld      c,l
        xor     a,a
        ld      (#MAPCTRL),a            ; ROM in slot 2: the stream's bank
        inc     hl
        ld      a,(hl)
        ld      (#MAPSLOT2),a
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl                   ; hl = v->p
ss_loop:
        ld      a,(hl)                  ; b = *p++
        inc     hl
        or      a,a
        jr      z,ss_zero
        jp      p,ss_wait
        ;; a command: channel ch = bits 5-6, vol[ch] = low 4 bits
        ld      e,a                     ; e = b
        push    hl
        rlca
        rlca
        rlca
        and     a,#3
        ld      d,a                     ; d = ch
        add     a,#V_VOL
        ld      l,a
        ld      h,#0
        add     hl,bc                   ; &v->vol[ch]
        ld      a,e
        and     a,#15
        ld      (hl),a
        pop     hl                      ; hl = p
        bit     4,e
        jr      z,ss_loop               ; (no data)
        ld      a,d
        cp      a,#3
        jr      z,ss_nctl
        add     a,a                     ; per[ch] = p[0] | p[1] << 8; p += 2
        add     a,#V_PER
        ex      de,hl                   ; de = p
        ld      l,a
        ld      h,#0
        add     hl,bc                   ; &v->per[ch]
        ld      a,(de)
        ld      (hl),a
        inc     de
        inc     hl
        ld      a,(de)
        ld      (hl),a
        inc     de
        ex      de,hl                   ; hl = p + 2
        jr      ss_loop
ss_nctl:
        ld      a,(hl)                  ; the noise control: v->nctl = *p++
        inc     hl
        push    hl
        ld      de,#V_NCTL
        ld      h,b
        ld      l,c
        add     hl,de
        ld      (hl),a
        pop     hl
        jr      ss_loop

ss_wait:                                ; a = the wait, hl = p
        ex      de,hl                   ; de = p
        ld      h,b
        ld      l,c
        inc     hl
        inc     hl                      ; &v->p
        ld      (hl),e
        inc     hl
        ld      (hl),d
        inc     hl
        ld      (hl),a                  ; v->wait = b
        ret

ss_zero:                                ; hl = p, at the two bytes of the offset
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      a,d
        or      a,e
        jr      z,ss_end
        dec     hl                      ; hl = p (just after the 0 byte)
        ld      a,e                     ; p = p - 1 - off = p + ~off
        cpl
        ld      e,a
        ld      a,d
        cpl
        ld      d,a
        add     hl,de
        jr      ss_loop

ss_end:                                 ; the end of the stream: off, silent (every volume 15); v->p stays after the 0
        dec     hl
        ex      de,hl                   ; de = p
        ld      h,b
        ld      l,c
        xor     a,a
        ld      (hl),a                  ; v->on = 0
        inc     hl
        inc     hl                      ; &v->p
        ld      (hl),e
        inc     hl
        ld      (hl),d
        ld      de,#V_VOL - V_P - 1
        add     hl,de                   ; &v->vol[0]
        ld      a,#15
        ld      (hl),a
        inc     hl
        ld      (hl),a
        inc     hl
        ld      (hl),a
        inc     hl
        ld      (hl),a
        ret

;; void snd_mix(void): the PSG gets what differs from what it has (cur_per, cur_vol, cur_nctl).  Channels 0 and 1
;; follow the music; 2 and the noise channel 3 follow the effect while it plays.  While a sampled voice plays
;; (pcm_on) channels 1 and 2 are the sample's.  Per channel: the period (tones) or noise control, then the volume.
_snd_mix::
        ld      hl,#_mus + V_PER        ; channel 0
        ld      de,#_cur_per
        ld      c,#0x80
        call    mx_tone
        ld      hl,#_mus + V_VOL
        ld      de,#_cur_vol
        ld      c,#0x90
        call    mx_vol
        ld      a,(_pcm_on)
        or      a,a
        jr      nz,mx_c2
        ld      hl,#_mus + V_PER + 2    ; channel 1
        ld      de,#_cur_per + 2
        ld      c,#0xA0
        call    mx_tone
        ld      hl,#_mus + V_VOL + 1
        ld      de,#_cur_vol + 1
        ld      c,#0xB0
        call    mx_vol
mx_c2:  ld      hl,#_mus                ; hl = the voice of channels 2 and 3
        ld      a,(_sfx + V_ON)
        or      a,a
        jr      z,1$
        ld      hl,#_sfx
1$:     ld      a,(_pcm_on)
        or      a,a
        jr      nz,mx_c3
        push    hl
        ld      de,#V_PER + 4           ; channel 2
        add     hl,de
        ld      de,#_cur_per + 4
        ld      c,#0xC0
        call    mx_tone
        pop     hl
        push    hl
        ld      de,#V_VOL + 2
        add     hl,de
        ld      de,#_cur_vol + 2
        ld      c,#0xD0
        call    mx_vol
        pop     hl
mx_c3:  push    hl                      ; channel 3: the noise control, then the volume
        ld      de,#V_NCTL
        add     hl,de
        ld      a,(hl)
        ld      hl,#_cur_nctl
        cp      a,(hl)
        jr      z,2$
        ld      (hl),a
        and     a,#7
        or      a,#0xE0
        out     (#PSG),a
2$:     pop     hl
        ld      de,#V_VOL + 3
        add     hl,de
        ld      de,#_cur_vol + 3
        ld      c,#0xF0
                                        ; (mx_vol)
;; mx_vol: hl = &voice vol[ch], de = &cur_vol[ch], c = 0x90 | ch << 5: the volume command when it changed
mx_vol:
        ld      a,(de)
        cp      a,(hl)
        ret     z
        ld      a,(hl)
        ld      (de),a
        or      a,c
        out     (#PSG),a
        ret

;; mx_tone: hl = &voice per[ch], de = &cur_per[ch], c = 0x80 | ch << 5: the period command when it changed:
;; low 4 bits, then bits 4-9
mx_tone:
        ld      a,(de)
        cp      a,(hl)
        jr      nz,1$
        inc     hl
        inc     de
        ld      a,(de)
        cp      a,(hl)
        ret     z
        dec     hl
        dec     de
1$:     ld      a,(hl)                  ; lo
        ld      (de),a
        inc     hl
        inc     de
        ld      b,(hl)                  ; hi
        ld      l,a                     ; l = lo
        ld      a,b
        ld      (de),a
        ld      a,l
        and     a,#15
        or      a,c
        out     (#PSG),a
        ld      a,b
        rlca
        rlca
        rlca
        rlca
        and     a,#0x30                 ; (hi & 3) << 4
        ld      b,a
        ld      a,l
        rrca
        rrca
        rrca
        rrca
        and     a,#0x0F                 ; lo >> 4
        or      a,b
        out     (#PSG),a
        ret
