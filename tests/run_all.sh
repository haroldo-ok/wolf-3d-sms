#!/bin/bash
# build the three ROMs (release, DEBUG, DET) and run every test script; exit status 0 = all passed
cd "$(dirname "$0")/.."
export PATH=$HOME/.devkitsms/bin:$PATH
mkdir -p /tmp/s
[ -x tools/smstest/smstest ] || make -C tools/smstest >/dev/null || { echo "cannot build tools/smstest"; exit 1; }
(make && make DEBUG=1 && make DET=1) >/tmp/s/build.log 2>&1 || { echo "BUILD FAILED"; tail -30 /tmp/s/build.log; exit 1; }   # SDCC warnings about the translated code go to the log
fail=0
for t in tests/t_*.txt; do
    case $t in *t_save*) continue;; esac       # needs the DET build: below
    rom=wolf3d_dbg.sms; case $t in *t_release*|*t_front*) rom=wolf3d.sms;; esac
    out=$(bash tools/st.sh $rom $t 2>&1)
    if echo "$out" | grep -q "RESULT: ALL PASSED"; then echo "PASS $t"
    else echo "FAIL $t"; echo "$out" | grep -i "fail" | head -5; fail=1; fi
done
# equivalence of the optimised code with the recorded behaviour: every frame of 8 scripted scenarios (screen and
# game state) on the deterministic build; the self checks of src/dbg.c (asm routines against their C originals)
if python3 tests/det/det.py | tee /tmp/s/det.out | grep -q FAIL; then echo "FAIL tests/det/det.py"; grep FAIL /tmp/s/det.out; fail=1; else echo "PASS tests/det/det.py (8 scenarios, frame by frame)"; fi
out=$(bash tools/st.sh wolf3d_det.sms tests/t_save.txt 2>&1)
if echo "$out" | grep -q "RESULT: ALL PASSED"; then echo "PASS tests/t_save.txt (save and load: 10 levels, the whole state hash)"; else echo "FAIL tests/t_save.txt"; echo "$out" | grep -i fail | head -5; fail=1; fi
if python3 tests/t_audio.py >/tmp/s/audio.out 2>&1; then echo "PASS tests/t_audio.py (title music, a voice by the line interrupt, an effect)"; else echo "FAIL tests/t_audio.py"; grep FAIL /tmp/s/audio.out; fail=1; fi
if python3 tests/t_scores.py >/tmp/s/scores.out 2>&1; then echo "PASS tests/t_scores.py (a high score survives a reset)"; else echo "FAIL tests/t_scores.py"; grep FAIL /tmp/s/scores.out; fail=1; fi
cat > /tmp/s/selfcheck.txt <<EOF
symbols build_det/game.noi
run 200
waituntil game_frames >= 2 20000 w2
expect dbg_actchk == 0 w2
expect dbg_mulchk == 0 w2
expect dbg_stepchk == 0 w2
expect dbg_xfchk == 0 w2
expect dbg_addchk == 0 w2
expect dbg_ivchk == 0 w2
expect dbg_sndchk == 0 w2
expect dbg_bkchk == 0 w2
expect dbg_umulchk == 0 w2
expect dbg_tmchk == 0 w2
expect dbg_tpchk == 0 w2
EOF
out=$(bash tools/st.sh wolf3d_det.sms /tmp/s/selfcheck.txt 2>&1)
if echo "$out" | grep -q "RESULT: ALL PASSED"; then echo "PASS self checks (act, mul, step, xform, add_sprite, in_view, snd, buckets, umul, trymove, tpath)"; else echo "FAIL self checks"; echo "$out" | grep -i fail | head; fail=1; fi
if python3 tests/check_view.py /tmp/s/t_release.ppm | tail -1 | grep -q PASS; then echo "PASS tests/check_view.py"; else echo "FAIL tests/check_view.py"; fail=1; fi
exit $fail
