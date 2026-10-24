#!/bin/bash
# smstest with banked-call attribution for the profiler.
# usage: tools/st.sh <rom> <script>   (*_dbg.sms uses build_dbg/ "make DEBUG=1", *_det.sms uses build_det/ "make DET=1")
cd "$(dirname "$0")/.."
B=build
case "$1" in *_dbg.sms) B=build_dbg;; *_det.sms) B=build_det;; esac
export SMSTEST_BCALL=0x$(awk '/___sdcc_bcall_ehl/{print $1; exit}' $B/game.map | sed 's/^0*//')
exec tools/smstest/smstest "$@"
