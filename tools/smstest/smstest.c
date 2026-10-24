/* SPDX-License-Identifier: MIT -- Copyright (c) 2026 Haroldo-OK. See LICENSE. */
/*
 * smstest - minimal headless Sega Master System emulator for automated
 * testing of homebrew ROMs built with devkitSMS.
 *
 * Emulates: Z80 (superzazu core), Sega mapper, 8KB RAM, VDP mode 4
 * (VRAM/CRAM, frame+line IRQ, vcounter, full background+sprite renderer
 * for screenshots), joypad ports.  PSG writes are accepted and logged.
 *
 * Driven by a simple script language:
 *   symbols <file.noi>          load NoICE symbols (DEF _name 0xADDR)
 *   hold <btn[+btn...]|none>    set held pad buttons (up,down,left,right,1,2;
 *                               start = Game Gear Start, port 0x00 bit 7)
 *   pause                       press the console's PAUSE button (an NMI)
 *   psglog <file> / psgstop     record sound chip writes (frame, byte)
 *   tap <btns>                  hold for 2 frames, then release
 *   run <n>                     run n frames
 *   waituntil <sym> <op> <val> <maxframes> [w1|s8|w2|s16]
 *   expect <sym|0xADDR> <op> <val> [w1|s8|w2|s16]   (default w2 unsigned LE)
 *                               <sym> may be sym+N (decimal or 0x hex) for a
 *                               struct field / array element
 *   poke <sym|0xADDR> <val> [w1|s8|w2]
 *   print <sym> [w1|s8|w2|s16]
 *   screenshot <file.ppm>
 *   expectvdp display <0|1>
 *   expectcolors <n>            fail if the frame shows fewer than n colours
 *   expectpace <n>              fail if > n VRAM writes landed < pacemin cycles apart
 *                               during active display (prints the first 8 offenders)
 *   pacemin <cycles>            threshold for expectpace (default 27 = devkitSMS's
 *                               RST 18h spacing; 28 adds a cycle of margin)
 *   dumpstate <file>            VRAM + CRAM + VDP regs + RAM
 *   shots <prefix> <count> <every>  screenshot every N frames
 *   frames                      print the frame counter
 *   liverender <0|1>            1: draw each line when the beam reaches it (shows
 *                               mid-frame VRAM writes as hardware would; slower)
 *   spmin                       lowest stack pointer seen (stack/data collision check)
 *   waitchange <sym> <maxframes> [w1|w2]  run until the value of <sym> changes (e.g. the next game frame)
 *   profstart|profstop|profdump <file>   flat sampling profiler (per address)
 *   cgstart|cgstop|cgdump <file>         call-graph profiler (report: prof2.py)
 *   hooklog <sym> <file> / hookstop      hash the screen on every entry to <sym>
 *   timers                      per-id cycle totals from debug port 0x06
 *   echo <text>
 * Screenshots always use per-line palette and horizontal-scroll snapshots, so
 * line-interrupt raster effects (HUD splits, gradients) appear as on hardware.
 * Game Gear: a .gg ROM (or SMSTEST_GG=1) selects 12-bit CRAM, the Start button
 * and 160x144 screenshots of the visible LCD window.
 * Environment: SMSTEST_GG=1, SMSTEST_PACE=<cycles>, SMSTEST_RAM=zero (power on with zeroed RAM; default = garbage),
 *   SMSTEST_TRACE=<file> (bytes written to debug port 0x04 are streamed there).
 * Also emulates 32 KB of cartridge RAM (mapper 0xFFFC bits 2-3).
 * Exit code 0 = all expectations met.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "z80.h"

#define RAM_SIZE 0x2000
#define MAX_ROM  (4*1024*1024)  /* full Sega mapper: 256 x 16K banks */
#define LINES_PER_FRAME 262
#define CYCLES_PER_LINE 228
#define ACTIVE_LINES 192

static uint8_t rom[MAX_ROM];
static uint32_t rom_size, rom_banks;
static uint8_t ram[RAM_SIZE];
static uint8_t mapper[4] = {0, 0, 1, 2}; /* FFFC..FFFF */
static uint8_t sram[0x8000];        /* cartridge RAM: FFFC bit3 enables in slot 2, bit2 selects 16K half */
static z80 cpu;
static int cur_line;
static unsigned long pace_viol = 0, pace_last = 0, vram_writes = 0;
/* Minimum cycles between VRAM data writes during active display. Default 27:
 * devkitSMS's own SMS_crt0_RST18 (used by SMS_setTile & co.) spaces its two
 * writes exactly 27 cycles apart, documented as "VRAM SAFE on every GameGear".
 * Set SMSTEST_PACE=28 (or the `pacemin` command) for an extra cycle of margin. */
static unsigned long pace_min = 27;

static uint8_t vram[0x4000];
static uint8_t cram[32];
static uint16_t gg_cram[32];   /* Game Gear: 12-bit entries */
static uint8_t gg_latch;
static int gg_mode = 0;          /* SMSTEST_GG=1 or a .gg ROM */
static uint8_t gg_start = 0xFF;   /* port 0x00 bit 7, active low */
static uint8_t vdp_reg[16];
static uint16_t vdp_addr;
static uint8_t vdp_code, vdp_latch, vdp_first, vdp_buf;
static uint8_t vdp_status;          /* bit7 = frame irq flag */
static uint8_t vdp_line_pending;    /* line irq flag */
static uint8_t line_counter;
static uint8_t pad1_state = 0xFF;   /* active low: U D L R B1 B2 */
static FILE* psg_log = NULL;        /* sound chip writes, for checking music */
static unsigned long psg_writes = 0;

static unsigned long frame_no;
static int failures;

/* ------------------------------------------------------------------ */
static uint8_t mem_read(void* ud, uint16_t addr) {
    (void)ud;
    if (addr < 0x0400) return rom[addr];                 /* fixed first 1KB */
    if (addr < 0x4000) return rom[((mapper[1] % rom_banks) * 0x4000u) + addr];
    if (addr < 0x8000) return rom[((mapper[2] % rom_banks) * 0x4000u) + (addr - 0x4000)];
    if (addr < 0xC000) {
        if (mapper[0] & 0x08) return sram[((mapper[0] & 0x04) ? 0x4000 : 0) + (addr - 0x8000)];
        return rom[((mapper[3] % rom_banks) * 0x4000u) + (addr - 0x8000)];
    }
    return ram[addr & 0x1FFF];
}
static void mem_write(void* ud, uint16_t addr, uint8_t val) {
    (void)ud;
    if (addr >= 0xFFFC) mapper[addr - 0xFFFC] = val;
    if (addr >= 0xC000) ram[addr & 0x1FFF] = val;
    else if (addr >= 0x8000 && (mapper[0] & 0x08)) sram[((mapper[0] & 0x04) ? 0x4000 : 0) + (addr - 0x8000)] = val;
}

/* ------------------------------ VDP ------------------------------- */
static void vdp_ctrl_write(uint8_t v) {
    if (!vdp_first) { vdp_latch = v; vdp_first = 1; return; }
    vdp_first = 0;
    vdp_code = v >> 6;
    vdp_addr = ((v & 0x3F) << 8) | vdp_latch;
    if (vdp_code == 0) { vdp_buf = vram[vdp_addr & 0x3FFF]; vdp_addr++; }
    else if (vdp_code == 2) vdp_reg[v & 0x0F] = vdp_latch;
}
static void vdp_data_write(uint8_t v) {
    vdp_first = 0;
    vram_writes++;
    if ((vdp_reg[1] & 0x40) && cur_line < 192 && vdp_code != 3) {
        if (cpu.cyc - pace_last < pace_min) { if (pace_viol < 8) printf("pace: pc=%04X line=%d gap=%lu\n", cpu.pc, cur_line, cpu.cyc - pace_last); pace_viol++; }
    }
    pace_last = cpu.cyc;
    if (vdp_code == 3) {
        if (gg_mode) {
            if (!(vdp_addr & 1)) gg_latch = v;
            else gg_cram[(vdp_addr >> 1) & 0x1F] = (uint16_t)(gg_latch | (v << 8));
        } else cram[vdp_addr & 0x1F] = v;
    }
    else vram[vdp_addr & 0x3FFF] = v;
    vdp_addr = (vdp_addr + 1) & 0x3FFF;
    vdp_buf = v;
}
static uint8_t vdp_data_read(void) {
    uint8_t r = vdp_buf;
    vdp_first = 0;
    vdp_buf = vram[vdp_addr & 0x3FFF];
    vdp_addr = (vdp_addr + 1) & 0x3FFF;
    return r;
}
static uint8_t vdp_status_read(void) {
    uint8_t r = vdp_status;
    vdp_status = 0; vdp_line_pending = 0; vdp_first = 0;
    cpu.int_pending = false;
    return r;
}
static uint8_t vcount_read(void) {
    /* NTSC 192-line mode: 0x00-0xDA then 0xD5-0xFF */
    return (cur_line <= 0xDA) ? (uint8_t)cur_line : (uint8_t)(cur_line - 6);
}
static int vdp_irq_active(void) {
    if ((vdp_status & 0x80) && (vdp_reg[1] & 0x20)) return 1;
    if (vdp_line_pending && (vdp_reg[0] & 0x10)) return 1;
    return 0;
}

/* ------------------------------ I/O ------------------------------- */
static uint8_t port_in(z80* z, uint8_t port) {
    (void)z;
    if (port < 0x40) { if (gg_mode && port == 0x00) return gg_start; return 0xFF; }
    if (port < 0x80) return (port & 1) ? 0xFF /*hcount*/ : vcount_read();
    if (port < 0xC0) return (port & 1) ? vdp_status_read() : vdp_data_read();
    if (!(port & 1)) return pad1_state | 0xC0;   /* 0xDC: P1 all + P2 UD (unpressed) */
    return 0xFF;                                  /* 0xDD */
}
static unsigned long long dt_start[16], dt_acc[16], dt_cnt[16];
static unsigned long long dt_max[16];
static void port_out(z80* z, uint8_t port, uint8_t val) {
    (void)z;
    if (port == 0x04) {                                    /* debug trace byte stream (debug builds only) */
        static FILE *tf; static int init;
        if (!init) { const char *fn = getenv("SMSTEST_TRACE"); init = 1; if (fn) tf = fopen(fn, "wb"); }
        if (tf) fputc(val, tf);
        return;
    }
    if (port == 0x06) {                                    /* debug timers (debug builds only) */
        int id = (val >> 1) & 15;
        if (val & 1) dt_start[id] = cpu.cyc; else { unsigned long long d = cpu.cyc - dt_start[id]; dt_acc[id] += d; dt_cnt[id]++; if (d > dt_max[id]) dt_max[id] = d; }
        return;
    }
    if (port >= 0x40 && port < 0x80) {                    /* PSG */
        if (psg_log) {
            fprintf(psg_log, "%lu %02X\n", frame_no, val);
            psg_writes++;
        }
        return;
    }
    if (port >= 0x80 && port < 0xC0) {
        if (port & 1) vdp_ctrl_write(val); else vdp_data_write(val);
    }
}

/* ------------------------- sampling profiler ----------------------
 * Counts cycles against the PC, in 32-byte buckets, so the cost of a frame can
 * be attributed to functions via the linker symbols.  Code-removal timing is
 * misleading in a simulation (removing work changes what the game does), so
 * measure instead. */
#define PROF_BUCKETS 65536
#define PROF_SHIFT 0
static unsigned long prof[PROF_BUCKETS];
static int profiling = 0;
static unsigned sp_min = 0xFFFF; static unsigned sp_min_pc;

static void prof_reset(void) { memset(prof, 0, sizeof prof); }

static void prof_dump(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { printf("FAIL: cannot write %s\n", path); failures++; return; }
    for (int i = 0; i < PROF_BUCKETS; i++)
        if (prof[i]) fprintf(f, "%04X %lu\n", i << PROF_SHIFT, prof[i]);
    fclose(f);
    printf("profdump: %s (frame %lu)\n", path, frame_no);
}

/* ---- call-graph profiler (cgstart/cgstop/cgdump; report: prof2.py) ----
/* Exclusive: cycles per exact PC, bank-qualified for slot-1 code that is not bank 1
 * (key = bank<<16 | pc, the same convention SDCC uses for --codeseg BANKn symbols).
 * Inclusive: a shadow call stack driven by CALL/RST/interrupt entry and RET; each
 * function's cycles from entry to return (outermost activation only, so recursion is
 * not double counted), with interrupt-handler time subtracted from whatever it interrupted. */
static uint32_t prof_key(uint16_t pc) {
    if (pc >= 0x4000 && pc < 0x8000 && mapper[2] != 1) return ((uint32_t)mapper[2] << 16) | pc;
    return pc;
}
#define PX_MAX 0x1000000
static uint32_t* pexcl;                 /* lazily allocated [PX_MAX] */
typedef struct { uint32_t key; unsigned long calls, incl; int active; } FStat;
#define FS_N 16384
static FStat fstat[FS_N];
typedef struct { uint32_t key; uint16_t sp; unsigned long cyc0, isr0; } Frame;
typedef struct { uint32_t par, key; unsigned long calls, incl; } Edge;
#define ED_N 65536
static Edge edges[ED_N];
static Edge* ed_get(uint32_t par, uint32_t key) {
    uint32_t h = ((par * 2654435761u) ^ (key * 40503u)) & (ED_N - 1);
    while (edges[h].calls && (edges[h].key != key || edges[h].par != par)) h = (h + 1) & (ED_N - 1);
    edges[h].par = par; edges[h].key = key; return &edges[h];
}
static Frame cstack[1024]; static int csp;
static unsigned long isr_acc;
static int cg_on = 0;
static void frames_unwind(z80* z);
static void hook_ret(z80* z) { frames_unwind(z); }
static FStat* fs_get(uint32_t key) {
    uint32_t h = (key * 2654435761u) & (FS_N - 1);
    while (fstat[h].calls && fstat[h].key != key) h = (h + 1) & (FS_N - 1);
    fstat[h].key = key; return &fstat[h];
}
static uint32_t bcall_addr = 0xFFFFFFFF;
static void hook_call(z80* z, uint16_t target) {
    uint32_t key = prof_key(target);
    /* SDCC banked calls: attribute the trampoline frames to the callee */
    if (target == bcall_addr || target == bcall_addr + 17)
        key = ((uint32_t)z->e << 16) | ((uint16_t)z->h << 8) | z->l;
    FStat* s = fs_get(key); s->calls++; s->active++;
    if (csp < 1024) { cstack[csp].key = key; cstack[csp].sp = z->sp; cstack[csp].cyc0 = z->cyc; cstack[csp].isr0 = isr_acc; csp++; }
}
static void frames_unwind(z80* z) {
    while (csp > 0 && cstack[csp - 1].sp < z->sp) {
        Frame* f = &cstack[--csp];
        FStat* s = fs_get(f->key);
        unsigned long t = z->cyc - f->cyc0;
        int isr = (f->key == 0x38 || f->key == 0x66);
        if (!isr) t -= (isr_acc - f->isr0);
        if (--s->active == 0) s->incl += t;
        { Edge* e = ed_get(csp ? cstack[csp - 1].key : 0xFFFFFF, f->key); e->calls++; e->incl += t; }
        if (isr) isr_acc += t;
    }
}
static void cg_reset(void) {
    if (!pexcl) pexcl = calloc(PX_MAX, sizeof(uint32_t));
    memset(pexcl, 0, PX_MAX * sizeof(uint32_t));
    memset(fstat, 0, sizeof fstat); memset(edges, 0, sizeof edges); csp = 0; isr_acc = 0;
}
static unsigned long cg_cycles;
static void cg_dump(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { printf("FAIL: cannot write %s\n", path); failures++; return; }
    fprintf(f, "T %lu\n", cg_cycles);
    for (uint32_t i = 0; i < PX_MAX; i++) if (pexcl[i]) fprintf(f, "X %06X %u\n", i, pexcl[i]);
    for (int i = 0; i < FS_N; i++) if (fstat[i].calls) fprintf(f, "I %06X %lu %lu\n", fstat[i].key, fstat[i].calls, fstat[i].incl);
    for (int i = 0; i < ED_N; i++) if (edges[i].calls) fprintf(f, "E %06X %06X %lu %lu\n", edges[i].par, edges[i].key, edges[i].calls, edges[i].incl);
    fclose(f);
    printf("cgdump: %s (frame %lu)\n", path, frame_no);
}

static uint16_t hook_pc; static int hook_on; static void hook_fire(void);
/* --------------------------- frame loop --------------------------- */
static void run_line(void) {
    unsigned long target = cpu.cyc + CYCLES_PER_LINE;
    while (cpu.cyc < target) {
        if (vdp_irq_active() && cpu.iff1) cpu.int_pending = true;
        if (cpu.sp < sp_min && cpu.sp >= 0xC000) { sp_min = cpu.sp; sp_min_pc = cpu.pc; }
        if (hook_on && cpu.pc == hook_pc) hook_fire();
        if (cg_on) {
            unsigned long before = cpu.cyc;
            uint32_t key = prof_key(cpu.pc);
            z80_step(&cpu);
            pexcl[key] += cpu.cyc - before;
            cg_cycles += cpu.cyc - before;
            if (csp && cstack[csp - 1].sp < cpu.sp) frames_unwind(&cpu);   /* jp (hl) returns */
        } else if (profiling) {
            unsigned long before = cpu.cyc;
            uint16_t pc = cpu.pc;
            z80_step(&cpu);
            prof[(pc >> PROF_SHIFT) & (PROF_BUCKETS - 1)] += cpu.cyc - before;
        } else {
            z80_step(&cpu);
        }
    }
}
/* Per-scanline snapshots of the palette and horizontal scroll, taken as each
 * active line starts, so screenshots show raster effects (line-interrupt
 * palette or scroll changes) the way the display would. (From jazz-jackrabbit-gg.)
 * A register-8 write during line N applies from line N+1, as on the VDP. */
static uint8_t  line_cram[192][32];
static uint16_t line_gg_cram[192][32];
static uint8_t  line_hscroll[192];
static int      line_valid = 0;
static int      pal_line = -1;
/* SMSTEST_FLATPAL=1: draw every line with the CURRENT palette, ignoring the per-line snapshots, so a
 * palette write somewhere in the middle of a frame doesn't make the picture depend on exactly when (in
 * which scanline) the program wrote it: pixel-exact comparisons between builds of different speed */
static int      flat_pal = 0;
/* liverender 1 (from opensosaria-sms): draw each active line from the VRAM,
 * CRAM and registers as they are when the beam reaches it, so VRAM written
 * after the beam passed (or a name table swapped mid-frame) shows as on
 * hardware. Vertical scroll is latched once per frame, at line 0. Screenshots
 * then show the last complete frame. Slower; off by default. */
static int live_render = 0;
static uint8_t live_yscroll;
static int live_frame_ok = 0;
static uint8_t img[192][256][3];
static uint8_t img_live[192][256][3];
static void render_lines(int y0, int y1, uint8_t yscroll, uint8_t (*out)[256][3]);
static void run_frame(void) {
    for (cur_line = 0; cur_line < LINES_PER_FRAME; cur_line++) {
        if (cur_line < 192) {
            memcpy(line_cram[cur_line], cram, 32);
            memcpy(line_gg_cram[cur_line], gg_cram, sizeof gg_cram);
            line_hscroll[cur_line] = vdp_reg[8];
        }
        if (live_render) {
            if (cur_line == 0) live_yscroll = vdp_reg[9];
            if (cur_line < ACTIVE_LINES) render_lines(cur_line, cur_line, live_yscroll, img_live);
            if (cur_line == ACTIVE_LINES) live_frame_ok = 1;
        }
        if (cur_line <= ACTIVE_LINES) {
            if (cur_line == ACTIVE_LINES) vdp_status |= 0x80;   /* vblank */
            if (line_counter == 0) { vdp_line_pending = 1; line_counter = vdp_reg[10]; }
            else line_counter--;
        } else line_counter = vdp_reg[10];
        run_line();
    }
    frame_no++;
    line_valid = 1;
}

/* --------------------------- renderer ----------------------------- */
static void color_rgb_raw(uint8_t c, uint8_t* rgb) {
    rgb[0] = (uint8_t)((c & 3) * 85);
    rgb[1] = (uint8_t)(((c >> 2) & 3) * 85);
    rgb[2] = (uint8_t)(((c >> 4) & 3) * 85);
}
/* entry = palette index 0..31; pal_line selects that scanline's snapshot */
static void color_rgb(uint8_t entry, uint8_t* rgb) {
    if (gg_mode) {
        uint16_t c = (line_valid && pal_line >= 0 && !flat_pal) ? line_gg_cram[pal_line][entry & 31]
                                                    : gg_cram[entry & 31];
        rgb[0] = (uint8_t)((c & 15) * 17);
        rgb[1] = (uint8_t)(((c >> 4) & 15) * 17);
        rgb[2] = (uint8_t)(((c >> 8) & 15) * 17);
    } else color_rgb_raw((line_valid && pal_line >= 0 && !flat_pal) ? line_cram[pal_line][entry & 31]
                                                       : cram[entry & 31], rgb);
}
static void dump_state(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { printf("FAIL: cannot write %s\n", path); failures++; return; }
    fwrite(vram, 1, sizeof vram, f);
    fwrite(cram, 1, sizeof cram, f);
    fwrite(vdp_reg, 1, sizeof vdp_reg, f);
    fwrite(ram, 1, sizeof ram, f);        /* 8K work RAM, for inspecting game state */
    fclose(f);
    printf("dumpstate: %s (frame %lu)\n", path, frame_no);
}

static void render_image(void) {
    if (live_render && live_frame_ok) { memcpy(img, img_live, sizeof img); return; }
    render_lines(0, 191, vdp_reg[9], img);
}
static void render_screenshot(const char* path) {
    render_image();
    FILE* f = fopen(path, "wb");
    if (!f) { printf("FAIL: cannot write %s\n", path); failures++; return; }
    if (gg_mode) {   /* the Game Gear LCD shows the 160x144 window at (48,24) */
        fprintf(f, "P6\n160 144\n255\n");
        for (int y = 24; y < 168; y++) fwrite(&img[y][48][0], 1, 160 * 3, f);
    } else {
        fprintf(f, "P6\n256 192\n255\n");
        fwrite(img, 1, sizeof img, f);
    }
    fclose(f);
    printf("screenshot: %s (frame %lu)\n", path, frame_no);
}
static void render_lines(int y0, int y1, uint8_t yscroll, uint8_t (*out)[256][3]) {
    static uint8_t bg_over[192][256];   /* priority tile with a non-zero pixel */
    memset(bg_over[y0], 0, (size_t)(y1 - y0 + 1) * 256);
    uint16_t ntbase = (uint16_t)(vdp_reg[2] & 0x0E) << 10;
    uint16_t satbase = (uint16_t)(vdp_reg[5] & 0x7E) << 7;
    uint16_t sprtiles = (vdp_reg[6] & 0x04) ? 0x2000 : 0x0000;
    int spr16 = vdp_reg[1] & 0x02;
    uint8_t xscroll = vdp_reg[8];
    int display_on = vdp_reg[1] & 0x40;
    for (int y = y0; y <= y1; y++) {
        pal_line = y;
        for (int x = 0; x < 256; x++) color_rgb(16 + (vdp_reg[7] & 0x0F), out[y][x]);
    }
    int lock_v = vdp_reg[0] & 0x80;   /* R0 b7: cols 24-31 ignore vscroll */
    int lock_h = vdp_reg[0] & 0x40;   /* R0 b6: rows 0-1 ignore hscroll  */
    if (display_on) {
        for (int y = y0; y <= y1; y++) {
            pal_line = y;
            for (int x = 0; x < 256; x++) {
                int ys = (lock_v && x >= 192) ? 0 : yscroll;
                int xs = (lock_h && y < 16)   ? 0 : (line_valid || live_render ? line_hscroll[y] : xscroll);
                int sy = (y + ys) % 224;
                int sx = (x - xs) & 0xFF;
                uint16_t ent = ntbase + ((sy >> 3) * 32 + (sx >> 3)) * 2;
                uint16_t e = vram[ent] | (vram[ent + 1] << 8);
                uint16_t tile = e & 0x1FF;
                int hf = e & 0x200, vf = e & 0x400, pal = (e & 0x800) ? 16 : 0;
                int py = vf ? 7 - (sy & 7) : (sy & 7);
                int px = hf ? 7 - (sx & 7) : (sx & 7);
                uint16_t ta = tile * 32 + py * 4;
                int bit = 7 - px;
                uint8_t ci = ((vram[ta] >> bit) & 1) | (((vram[ta+1] >> bit) & 1) << 1)
                           | (((vram[ta+2] >> bit) & 1) << 2) | (((vram[ta+3] >> bit) & 1) << 3);
                color_rgb(pal + ci, out[y][x]);
                bg_over[y][x] = (e & 0x1000) && ci;   /* priority bit: hides sprites */
            }
        }
        int zoom = vdp_reg[1] & 0x01;   /* R1 b0: magnified sprites (2x in both directions) */
        int sh = (spr16 ? 16 : 8) << zoom;
        int nspr = 64;   /* SAT terminator */
        for (int i = 0; i < 64; i++) if (vram[satbase + i] == 0xD0) { nspr = i; break; }
        /* the VDP scans the SAT in order and displays at most 8 sprites per
         * scanline; the 9th and beyond are dropped on that line. Only the
         * lines being drawn are evaluated. */
        static uint8_t shown[64][192];
        int percount[192];
        for (int y = y0; y <= y1; y++) percount[y] = 0;
        for (int i = 0; i < nspr; i++) {
            int sy = vram[satbase + i] + 1;
            for (int y = y0; y <= y1; y++) {
                int ty = y - sy;
                shown[i][y] = 0;
                if (ty < 0 || ty >= sh) continue;
                if (percount[y] < 8) shown[i][y] = 1;
                percount[y]++;
            }
        }
        /* lower sprite index wins, so draw back to front */
        for (int i = nspr - 1; i >= 0; i--) {
            int sy = vram[satbase + i] + 1;
            int sx = vram[satbase + 128 + i * 2];
            uint16_t tile = vram[satbase + 128 + i * 2 + 1];
            if (spr16) tile &= 0xFE;
            for (int ty = 0; ty < sh; ty++) {
                int y = sy + ty; if (y < y0 || y > y1) continue;
                if (!shown[i][y]) continue;
                pal_line = y;
                int sty = ty >> zoom;
                uint16_t ta = sprtiles + (tile + (sty >> 3)) * 32 + (sty & 7) * 4;
                for (int tx = 0; tx < (8 << zoom); tx++) {
                    int x = sx + tx; if (x < 0 || x >= 256) continue;
                    if (bg_over[y][x]) continue;
                    int bit = 7 - (tx >> zoom);
                    uint8_t ci = ((vram[ta] >> bit) & 1) | (((vram[ta+1] >> bit) & 1) << 1)
                               | (((vram[ta+2] >> bit) & 1) << 2) | (((vram[ta+3] >> bit) & 1) << 3);
                    if (ci) color_rgb(16 + ci, out[y][x]);
                }
            }
        }
        if (vdp_reg[0] & 0x20)  /* left column blank */
            for (int y = y0; y <= y1; y++) {
                pal_line = y;
                for (int x = 0; x < 8; x++) color_rgb(16 + (vdp_reg[7] & 0x0F), out[y][x]);
            }
    }
    pal_line = -1;
}

/* hooklog <sym> <file>: on every entry to <sym>, hash the rendered screen (what the previous call left) */
static FILE* hook_f; static unsigned long hook_n;
static void hook_fire(void) {
    render_image();
    uint64_t h = 1469598103934665603ULL; const uint8_t* p = &img[0][0][0];
    for (size_t i = 0; i < sizeof img; i++) { h ^= p[i]; h *= 1099511628211ULL; }
    fprintf(hook_f, "%lu %016llx\n", hook_n++, (unsigned long long)h);
}
/* --------------------------- symbols ------------------------------ */
typedef struct { char name[64]; uint32_t addr; } Sym;
static Sym syms[4096]; static int nsyms;

static void load_symbols(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("FAIL: cannot open symbols %s\n", path); failures++; return; }
    char a[64], b[64], c[64];
    while (fscanf(f, "%63s %63s %63s", a, b, c) == 3) {
        if (strcmp(a, "DEF") == 0 && nsyms < 4096) {
            strncpy(syms[nsyms].name, b, 63);
            syms[nsyms].addr = (uint32_t)strtoul(c, NULL, 0);
            nsyms++;
        }
    }
    fclose(f);
    printf("symbols: loaded %d from %s\n", nsyms, path);
}
static int sym_lookup_base(const char* name, uint32_t* out);
/* "sym+N" (N decimal or 0x hex) addresses a field inside a struct or array. */
static int sym_lookup(const char* name, uint32_t* out) {
    const char* plus = strchr(name, '+');
    if (plus && plus != name) {
        char base[66];
        size_t n = (size_t)(plus - name);
        if (n >= sizeof base) return 0;
        memcpy(base, name, n); base[n] = 0;
        if (!sym_lookup_base(base, out)) return 0;
        *out += (uint32_t)strtoul(plus + 1, NULL, 0);
        return 1;
    }
    return sym_lookup_base(name, out);
}
static int sym_lookup_base(const char* name, uint32_t* out) {
    char alt[66];
    snprintf(alt, sizeof alt, "_%s", name);
    for (int i = 0; i < nsyms; i++)
        if (!strcmp(syms[i].name, name) || !strcmp(syms[i].name, alt)) {
            *out = syms[i].addr; return 1;
        }
    if (name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
        *out = (uint32_t)strtoul(name, NULL, 16); return 1;
    }
    return 0;
}

/* --------------------------- script ------------------------------- */
static uint32_t parse_buttons(const char* s) {
    uint32_t m = 0;
    char buf[128]; strncpy(buf, s, 127); buf[127] = 0;
    for (char* t = strtok(buf, "+"); t; t = strtok(NULL, "+")) {
        if (!strcmp(t, "up")) m |= 0x01;
        else if (!strcmp(t, "down")) m |= 0x02;
        else if (!strcmp(t, "left")) m |= 0x04;
        else if (!strcmp(t, "right")) m |= 0x08;
        else if (!strcmp(t, "1")) m |= 0x10;
        else if (!strcmp(t, "2")) m |= 0x20;
        else if (!strcmp(t, "start")) m |= 0x100;
        else if (!strcmp(t, "none")) m |= 0;
        else printf("WARN: unknown button '%s'\n", t);
    }
    return m;
}
static void set_pad(uint32_t mask) { pad1_state = (uint8_t)(~mask); gg_start = (mask & 0x100) ? 0x7F : 0xFF; }

static long read_val(uint32_t addr, const char* width) {
    if (!strcmp(width, "w1")) return ram[addr & 0x1FFF];
    if (!strcmp(width, "s8")) return (int8_t)ram[addr & 0x1FFF];
    long v = ram[addr & 0x1FFF] | (ram[(addr + 1) & 0x1FFF] << 8);
    if (!strcmp(width, "s16") && v >= 0x8000) v -= 0x10000;
    return v;
}
static int cmp_op(long a, const char* op, long b) {
    if (!strcmp(op, "==")) return a == b;
    if (!strcmp(op, "!=")) return a != b;
    if (!strcmp(op, "<"))  return a < b;
    if (!strcmp(op, ">"))  return a > b;
    if (!strcmp(op, "<=")) return a <= b;
    if (!strcmp(op, ">=")) return a >= b;
    printf("WARN: bad op %s\n", op);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s rom.sms script.txt\n", argv[0]); return 2; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("rom"); return 2; }
    /* Real hardware and most emulators do NOT power on with RAM cleared.
     * SMSTEST_RAM=zero keeps the old behaviour; the default is a pattern, so
     * code that assumes zeroed memory fails here too. (From flashback-sms.) */
    {
        { const char *bc = getenv("SMSTEST_BCALL"); if (bc) bcall_addr = strtoul(bc, NULL, 0); }
        { const char *fp = getenv("SMSTEST_FLATPAL"); if (fp && *fp == '1') flat_pal = 1; }
        const char *pm = getenv("SMSTEST_PACE"); if (pm) pace_min = strtoul(pm, NULL, 0);
        const char *mode = getenv("SMSTEST_RAM");
        if (!mode || strcmp(mode, "zero")) {
            unsigned seed = 0x1992;
            for (int i = 0; i < RAM_SIZE; i++) { seed = seed * 1103515245u + 12345u; ram[i] = (uint8_t)(seed >> 16); }
        }
    }
    rom_size = (uint32_t)fread(rom, 1, MAX_ROM, f);
    fclose(f);
    rom_banks = rom_size / 0x4000; if (!rom_banks) rom_banks = 1;
    printf("rom: %s (%u bytes, %u banks)\n", argv[1], rom_size, rom_banks);
    { const char* e = getenv("SMSTEST_GG"); size_t L = strlen(argv[1]);
      if ((e && *e == '1') || (L > 3 && !strcmp(argv[1] + L - 3, ".gg"))) { gg_mode = 1; printf("mode: Game Gear\n"); } }

    z80_init(&cpu);
    cpu.read_byte = mem_read; cpu.write_byte = mem_write;
    cpu.port_in = port_in; cpu.port_out = port_out;

    FILE* sf = fopen(argv[2], "r");
    if (!sf) { perror("script"); return 2; }
    char line[512];
    while (fgets(line, sizeof line, sf)) {
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        if (!line[0] || line[0] == '#') continue;
        char c1[128], c2[128], c3[128], c4[128], c5[128], c6[128];
        int n = sscanf(line, "%127s %127s %127s %127s %127s %127s", c1, c2, c3, c4, c5, c6);
        if (n < 1) continue;
        if (!strcmp(c1, "symbols") && n >= 2) load_symbols(c2);
        else if (!strcmp(c1, "hold") && n >= 2) set_pad(parse_buttons(c2));
        else if (!strcmp(c1, "tap") && n >= 2) {
            set_pad(parse_buttons(c2)); run_frame(); run_frame();
            set_pad(0); run_frame();
        }
        else if (!strcmp(c1, "run") && n >= 2) {
            long k = atol(c2);
            while (k-- > 0) run_frame();
        }
        else if (!strcmp(c1, "waituntil") && n >= 5) {
            uint32_t addr; long maxf = atol(c5);
            const char* w = (n >= 6) ? c6 : "w2";
            if (!sym_lookup(c2, &addr)) { printf("FAIL: no symbol %s\n", c2); failures++; continue; }
            long target = strtol(c4, NULL, 0);
            long i;
            for (i = 0; i < maxf; i++) {
                if (cmp_op(read_val(addr, w), c3, target)) break;
                run_frame();
            }
            if (i >= maxf) {
                printf("FAIL: waituntil %s %s %ld timed out after %ld frames (val=%ld)\n",
                       c2, c3, target, maxf, read_val(addr, w));
                failures++;
            } else printf("ok: waituntil %s %s %ld (after %ld frames)\n", c2, c3, target, i);
        }
        else if (!strcmp(c1, "waitchange") && n >= 3) {
            /* waitchange SYM MAXFRAMES [w1|w2]: run until the value of SYM differs from what it is now (a
             * counter's next increment: "a frame of the game just started") */
            uint32_t addr; long maxf = atol(c3);
            const char* w = (n >= 4) ? c4 : "w2";
            if (!sym_lookup(c2, &addr)) { printf("FAIL: no symbol %s\n", c2); failures++; continue; }
            long v0 = read_val(addr, w), i;
            for (i = 0; i < maxf; i++) {
                run_frame();
                if (read_val(addr, w) != v0) break;
            }
            if (i >= maxf) { printf("FAIL: waitchange %s timed out after %ld frames (val=%ld)\n", c2, maxf, v0); failures++; }
            else printf("ok: waitchange %s (%ld -> %ld after %ld frames)\n", c2, v0, read_val(addr, w), i + 1);
        }
        else if (!strcmp(c1, "expect") && n >= 4) {
            uint32_t addr;
            const char* w = (n >= 5) ? c5 : "w2";
            if (!sym_lookup(c2, &addr)) { printf("FAIL: no symbol %s\n", c2); failures++; continue; }
            long v = read_val(addr, w), target = strtol(c4, NULL, 0);
            if (cmp_op(v, c3, target)) printf("ok: %s = %ld %s %ld\n", c2, v, c3, target);
            else { printf("FAIL: %s = %ld, expected %s %ld\n", c2, v, c3, target); failures++; }
        }
        else if (!strcmp(c1, "poke") && n >= 3) {
            uint32_t addr;
            if (!sym_lookup(c2, &addr)) { printf("FAIL: no symbol %s\n", c2); failures++; continue; }
            long v = strtol(c3, NULL, 0);
            ram[addr & 0x1FFF] = (uint8_t)(v & 0xFF);
            if (n < 4 || (strcmp(c4, "w1") && strcmp(c4, "s8"))) ram[(addr + 1) & 0x1FFF] = (uint8_t)((v >> 8) & 0xFF);
            printf("poke: %s <- %ld\n", c2, v);
        }
        else if (!strcmp(c1, "print") && n >= 2) {
            uint32_t addr;
            const char* w = (n >= 3) ? c3 : "w2";
            if (sym_lookup(c2, &addr))
                printf("print: %s = %ld (frame %lu)\n", c2, read_val(addr, w), frame_no);
        }
        else if (!strcmp(c1, "screenshot") && n >= 2) render_screenshot(c2);
        else if (!strcmp(c1, "liverender") && n >= 2) { live_render = atoi(c2); live_frame_ok = 0; printf("liverender: %d\n", live_render); }
        else if (!strcmp(c1, "dumpstate") && n >= 2) dump_state(c2);
        else if (!strcmp(c1, "pause")) { z80_gen_nmi(&cpu); run_frame(); printf("pause (frame %lu)\n", frame_no); }
        else if (!strcmp(c1, "psglog") && n >= 2) {
            psg_log = fopen(c2, "w");
            printf("psglog: %s (frame %lu)\n", c2, frame_no);
        }
        else if (!strcmp(c1, "psgstop")) {
            if (psg_log) { fclose(psg_log); psg_log = NULL; }
            printf("psgstop: %lu writes (frame %lu)\n", psg_writes, frame_no);
        }
        else if (!strcmp(c1, "spmin")) printf("spmin: %04X (pc %04X)\n", sp_min, sp_min_pc);
        else if (!strcmp(c1, "profstart")) { prof_reset(); profiling = 1; printf("profstart (frame %lu)\n", frame_no); }
        else if (!strcmp(c1, "profstop")) { profiling = 0; printf("profstop (frame %lu)\n", frame_no); }
        else if (!strcmp(c1, "profdump") && n >= 2) prof_dump(c2);
        else if (!strcmp(c1, "cgstart")) { cg_reset(); cg_cycles = 0; cg_on = 1; cpu.on_call = hook_call; cpu.on_ret = hook_ret; printf("cgstart (frame %lu)\n", frame_no); }
        else if (!strcmp(c1, "cgstop")) { cg_on = 0; cpu.on_call = NULL; cpu.on_ret = NULL; printf("cgstop (frame %lu)\n", frame_no); }
        else if (!strcmp(c1, "cgdump") && n >= 2) cg_dump(c2);
        else if (!strcmp(c1, "hooklog") && n >= 3) {
            uint32_t a; if (!sym_lookup(c2, &a)) { printf("FAIL: no symbol %s\n", c2); failures++; continue; }
            hook_pc = (uint16_t)a; hook_f = fopen(c3, "w"); hook_on = hook_f != NULL; hook_n = 0;
            printf("hooklog: %s @%04X -> %s\n", c2, hook_pc, c3);
        }
        else if (!strcmp(c1, "hookstop")) { if (hook_f) fclose(hook_f); hook_f = NULL; hook_on = 0; printf("hookstop: %lu entries\n", hook_n); }
        else if (!strcmp(c1, "frames")) printf("frames: %lu\n", frame_no);
        else if (!strcmp(c1, "expectcolors") && n >= 2) {
            /* Guards against "passing" on a blank/black frame: renders the
               current frame and counts distinct colours actually shown. */
            render_screenshot("/tmp/_smstest_check.ppm");
            FILE* pf = fopen("/tmp/_smstest_check.ppm", "rb");
            int distinct = 0;
            if (pf) {
                unsigned char seen[1 << 24 >> 16] = {0};  /* unused */
                (void)seen;
                char hdr[64];
                if (fgets(hdr, sizeof hdr, pf) && fgets(hdr, sizeof hdr, pf) && fgets(hdr, sizeof hdr, pf)) {
                    static unsigned char pal[256][3];
                    unsigned char px3[3];
                    while (fread(px3, 1, 3, pf) == 3) {
                        int found = 0;
                        for (int k = 0; k < distinct; k++)
                            if (!memcmp(pal[k], px3, 3)) { found = 1; break; }
                        if (!found && distinct < 256) { memcpy(pal[distinct], px3, 3); distinct++; }
                    }
                }
                fclose(pf);
            }
            int want = atoi(c2);
            if (distinct >= want) printf("ok: frame shows %d distinct colours (>= %d)\n", distinct, want);
            else { printf("FAIL: frame shows only %d distinct colours, expected >= %d (blank screen?)\n",
                          distinct, want); failures++; }
        }
        else if (!strcmp(c1, "expectvdp") && n >= 3 && !strcmp(c2, "display")) {
            int on = (vdp_reg[1] & 0x40) ? 1 : 0;
            if (on == atoi(c3)) printf("ok: vdp display %d\n", on);
            else { printf("FAIL: vdp display=%d expected %s\n", on, c3); failures++; }
        }
        else if (!strcmp(c1, "timers")) {
            for (int i = 0; i < 16; i++) if (dt_cnt[i]) printf("timer %d: %llu calls, %llu cycles, %llu avg, %llu max\n", i, dt_cnt[i], dt_acc[i], dt_acc[i] / dt_cnt[i], dt_max[i]);
        }
        else if (!strcmp(c1, "pacemin") && n >= 2) { pace_min = strtoul(c2, NULL, 0); printf("pacemin: %lu cycles\n", pace_min); }
        else if (!strcmp(c1, "expectpace") && n >= 2) {
            unsigned long lim = strtoul(c2, NULL, 0);
            if (pace_viol <= lim) printf("ok: %lu fast VRAM writes during active display (<= %lu), %lu writes total\n", pace_viol, lim, vram_writes);
            else { printf("FAIL: %lu VRAM writes <%lu cycles apart during active display\n", pace_viol, pace_min); failures++; }
        }
        else if (!strcmp(c1, "shots") && n >= 4) {
            /* shots <prefix> <count> <every>: run and grab a screenshot every N frames */
            int cntn = atoi(c3), every = atoi(c4); char nm[256];
            for (int i = 0; i < cntn; i++) { for (int k = 0; k < every; k++) run_frame(); snprintf(nm, sizeof nm, "%s%04d.ppm", c2, i); render_screenshot(nm); }
        }
        else if (!strcmp(c1, "echo")) printf("== %s\n", line + 5);
        else printf("WARN: bad command: %s\n", line);
    }
    fclose(sf);
    printf(failures ? "RESULT: %d FAILURE(S)\n" : "RESULT: ALL PASSED\n", failures);
    return failures ? 1 : 0;
}
