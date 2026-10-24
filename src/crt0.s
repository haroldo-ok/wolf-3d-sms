;; crt0 for Wolfenstein 3D SMS (from the crt0 of catacomb-abyss-sms, itself derived from devkitSMS crt0_sms.s by sverx)
        .module crt0
        .globl _main
        .globl _isr_vblank
        .globl _nmi_handler

        .area _HEADER (ABS)
        .org    0
        di
        im 1
        ld sp, #0xdff0
        jr crt0_init

        .org    0x08
_vdp_ctrl_hl::                          ; write HL to VDP control port (irq safe)
        ld c, #0xBF
        di
        out (c),l
        out (c),h
        ei
        ret

        .org    0x18
_vdp_data_hl::                          ; write HL to data port, 27 cycles apart
        ld a,l
        out (#0xBE),a
        ld a,h
        inc hl
        dec hl
        out (#0xBE),a
        ret

crt0_init:
        ld hl,#0x0000
        ld (#0xfffc),hl
        ld hl,#0x0201
        ld (#0xfffe),hl
        jr crt0_clear_ram

        .org    0x30
get_bank::
        ld a,(#0xfffe)
        ret
set_bank::
        ld (#0xfffe),a
        ret

        .org    0x38
        jp _isr_vblank

crt0_clear_ram:
        ld hl,#0xc000
        ld (hl),#0
        ld de,#0xc001
        ld bc,#0x1fef                   ; clear all of RAM below the stack area
        ldir
        call crt0_gsinit
        call _main
1$:     halt
        jr 1$

        .org    0x66
        jp _nmi_handler

        ;; Ordering of segments for the linker.
        .area   _HOME
        .area   _CODE
        .area   _INITIALIZER
        .area   _GSINIT
        .area   _GSFINAL

        .area   _DATA
        .area   _INITIALIZED
        .area   _BSEG
        .area   _BSS
        .area   _HEAP

        .area   _CODE
        .area   _GSINIT
crt0_gsinit::
        ld bc, #l__INITIALIZER
        ld a, b
        or a, c
        jr Z, crt0_gsinit_next
        ld de, #s__INITIALIZED
        ld hl, #s__INITIALIZER
        ldir
crt0_gsinit_next:
        .area   _GSFINAL
        ret
