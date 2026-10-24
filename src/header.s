        .module segahdr
        .area _SEGAHDR (ABS)
        .org 0x7FF0
        .ascii "TMR SEGA"
        .db 0xFF, 0xFF
        .dw 0x0000              ; checksum (filled by makesms)
        .db 0x99, 0x99          ; product code
        .db 0x00                ; version
        .db 0x4C                ; SMS export, 32 KiB checksum range
