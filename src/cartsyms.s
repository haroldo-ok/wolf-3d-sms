;; cartsyms.s - the arrays of cartridge RAM (layout: src/cartram.h; level.c checks it)
        .module cartsyms
        .globl  _tilemap, _actorat, _objlist, _statobjlist, _doorobjlist, _doorposition
_tilemap = 0x8000
_actorat = 0x9000
_objlist = 0xA000
_statobjlist = 0xB650
_doorobjlist = 0xBC00
_doorposition = 0xBB80
