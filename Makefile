# Wolfenstein 3D for the Sega Master System
DK      ?= $(HOME)/.devkitsms
# SDCC 4.4.0: 4.5.0 miscompiles "if (a != g) { g = a; ... }" (see README)
SDCCBIN ?= $(HOME)/sdcc440/sdcc-4.4.0/bin
CC      = $(SDCCBIN)/sdcc
# --reserve-regs-iy: SDCC 4.4.0 reuses IY after reloading it ("ld iy,#_pluy" then "2(iy)" meant for player->x):
# silently wrong code, e.g. player->tilex.  IY is never used by compiled code.
CFLAGS  = -mz80 --reserve-regs-iy -Isrc -Isrc/game -Igen --peep-file $(DK)/include/peep-rules.txt --opt-code-speed
AS      = $(SDCCBIN)/sdasz80
OUT     = build
ROM     = wolf3d.sms
# make DEBUG=1: the test hooks of render.c (dbg_*) in build_dbg/ -> wolf3d_dbg.sms
DEBUG   ?= 0
ifeq ($(DEBUG),1)
CFLAGS += -DDEBUG_MIRRORS
OUT     = build_dbg
ROM     = wolf3d_dbg.sms
endif
# make DET=1: a deterministic build for the equivalence tests (tests/det): the debug hooks plus a fixed
# time step per frame, constant random seed, binary pad, and dbg_hash = a hash of the whole game state
DET     ?= 0
ifeq ($(DET),1)
CFLAGS += -DDEBUG_MIRRORS -DDEBUG_DET
OUT     = build_det
ROM     = wolf3d_det.sms
endif
# first bank of the packed data (gen/data.bin); code banks are 1..DATA_BANK-1
DATA_BANK = 16
export PATH := $(DK)/bin:$(PATH)

# bank 0 (fixed) sources
FIXED   = src/main.c src/hw.c src/snd.c src/render.c src/mathz.c src/level.c src/plat0.c \
          gen/assets_tab.c gen/states.c
# banked sources (code and constants in the same bank)
BANK1   = src/game/wl_play.c src/game/wl_main.c src/game/wl_game.c gen/rndtable.c
BANK2   = src/game/wl_agent.c
BANK3   = src/game/wl_state.c src/game/wl_act1.c
BANK4   = src/game/wl_act2.c
BANK5   = src/vid.c src/hud.c src/input.c src/weapon.c src/inter.c $(if $(filter 1,$(DET)),src/dbg.c)
BANK6   = src/scr.c src/menu.c gen/scr_tab.c
BANK7   = src/save.c
BANK8   = src/snd_ctl.c gen/snd_tab.c
BANK9   = $(if $(filter 1,$(DET)),src/dbg2.c)
BANK10  =
BANK11  =
BANK12  =
BANK13  =
BANK14  =
BANK15  =
BANKS   = $(foreach b,1 2 3 4 5 6 7 8 9 10 11 12 13 14 15,$(if $(BANK$(b)),$(b)))

rel = $(OUT)/$(notdir $(1:.c=.rel))
FIXED_REL  = $(foreach f,$(FIXED),$(call rel,$(f)))
BANKED_REL = $(foreach b,$(BANKS),$(foreach f,$(BANK$(b)),$(call rel,$(f))))
ASM_REL    = $(OUT)/crt0.rel $(OUT)/header.rel $(OUT)/render_a.rel $(OUT)/cartsyms.rel $(OUT)/mulfix.rel $(OUT)/fast.rel $(OUT)/sprites_a.rel $(OUT)/actors_a.rel $(OUT)/snd_a.rel $(OUT)/agent_a.rel
HDRS       = $(wildcard src/*.h src/game/*.h) gen/assets.h

all: $(ROM)

$(OUT):
	mkdir -p $(OUT)

# the converters (tools/build_assets.py) need the id data (tools/fetch_data.sh); make NOGEN=1 builds from the
# generated files of gen/ that come with the sources
ifneq ($(NOGEN),1)
gen/assets.h gen/assets.inc gen/assets_tab.c gen/rndtable.c gen/data.bin: tools/*.py src/cartram.h
	cd tools && python3 build_assets.py
endif

$(OUT)/crt0.rel: src/crt0.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/mulfix.rel: src/mulfix.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/fast.rel: src/fast.s gen/assets.inc | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/sprites_a.rel: src/sprites_a.s gen/assets.inc | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/actors_a.rel: src/actors_a.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/agent_a.rel: src/agent_a.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/snd_a.rel: src/snd_a.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/cartsyms.rel: src/cartsyms.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/header.rel: src/header.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/render_a.rel: src/render_a.s gen/assets.inc | $(OUT)
	$(AS) -plosgff -o $@ $<

define FIXED_RULE
$(call rel,$(1)): $(1) $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -c -o $$@ $$<
endef
define BANK_RULE
$(call rel,$(1)): $(1) $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) --codeseg BANK$(2) --constseg BANK$(2) -c -o $$@ $$<
endef
$(foreach f,$(FIXED),$(eval $(call FIXED_RULE,$(f))))
$(foreach b,$(BANKS),$(foreach f,$(BANK$(b)),$(eval $(call BANK_RULE,$(f),$(b)))))

BANK_LOCS = $(foreach b,$(BANKS),-Wl-b_BANK$(b)=0x$(b)4000)

$(OUT)/game.ihx: $(ASM_REL) $(FIXED_REL) $(BANKED_REL)
	$(CC) -mz80 --no-std-crt0 -Wl-u --code-loc 0x0080 --data-loc 0xC000 $(BANK_LOCS) \
	    -o $@ $(ASM_REL) $(FIXED_REL) $(BANKED_REL)

$(ROM): $(OUT)/game.ihx gen/data.bin
	makesms -mbin gen/data.bin:0:$$(stat -c %s gen/data.bin):$$(($(DATA_BANK)*16384)) $(OUT)/game.ihx $(ROM)
	python3 tools/checkrom.py $(OUT)/game.map $(ROM) $(DATA_BANK)

clean:
	rm -rf $(OUT) $(ROM)
