#!/bin/bash
# The sources as a zip: everything the build needs except what it can get or make itself.  Left out: the objects
# (build*/), the ROMs, gen/ (made by `make` from the id data; only gen/states.c and states.h are kept, nothing in
# the build makes them), the emulator binary, __pycache__.  The id data is not in it either: tools/fetch_data.sh.
# usage: tools/make_zip.sh [OUT.zip]      (default: wolf3d-sms-src.zip next to the project directory)
set -euo pipefail
cd "$(dirname "$0")/.."
name="$(basename "$PWD")"
out="$(realpath -m "${1:-$PWD/../wolf3d-sms-src.zip}")"
rm -f "$out"
cd ..
{
    find "$name" -type f \
        ! -path "$name/build*" ! -path "$name/gen/*" ! -path "$name/work/*" \
        ! -path '*/__pycache__/*' ! -name '*.pyc' ! -name '*.sms' ! -path "$name/tools/smstest/smstest"
    echo "$name/gen/states.c"
    echo "$name/gen/states.h"
} | LC_ALL=C sort | zip -q -X "$out" -@
echo "$out"
unzip -l "$out" | tail -1
