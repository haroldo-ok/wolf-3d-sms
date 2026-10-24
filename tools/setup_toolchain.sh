#!/bin/bash
# Installs what the Makefile needs outside the project (about 25 MB to download; Linux x86-64, needs curl,
# tar, git and a C compiler):
#   SDCC 4.4.0  -> $SDCC_DIR/sdcc-4.4.0   (default ~/sdcc440).  The exact version: 4.5.0 miscompiles code
#                  this project relies on (see the Makefile and the README).  Do not use a distribution's sdcc.
#   makesms     -> $DK/bin                 (default ~/.devkitsms): devkitSMS's ROM packer, built from source,
#   peep-rules.txt -> $DK/include          and the peephole rules the compiler is given
# usage: tools/setup_toolchain.sh        then: make   (the Makefile looks in the same two places)
set -euo pipefail
SDCC_DIR="${SDCC_DIR:-$HOME/sdcc440}"
DK="${DK:-$HOME/.devkitsms}"
mkdir -p "$SDCC_DIR" "$DK/bin" "$DK/include"

if [ -x "$SDCC_DIR/sdcc-4.4.0/bin/sdcc" ]; then
    echo "SDCC 4.4.0 is already in $SDCC_DIR"
else
    echo "downloading SDCC 4.4.0 ..."
    curl -fL -o "$SDCC_DIR/sdcc-4.4.0.tar.bz2" \
        "https://sourceforge.net/projects/sdcc/files/sdcc-linux-amd64/4.4.0/sdcc-4.4.0-amd64-unknown-linux2.5.tar.bz2/download"
    tar xjf "$SDCC_DIR/sdcc-4.4.0.tar.bz2" -C "$SDCC_DIR"
    rm -f "$SDCC_DIR/sdcc-4.4.0.tar.bz2"
fi

if [ -x "$DK/bin/makesms" ] && [ -f "$DK/include/peep-rules.txt" ]; then
    echo "makesms and peep-rules.txt are already in $DK"
else
    echo "fetching devkitSMS ..."
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' EXIT
    git clone --depth 1 https://github.com/sverx/devkitSMS "$tmp/devkitSMS"
    "${CC:-cc}" -O2 -w -o "$DK/bin/makesms" "$tmp/devkitSMS/tools/makesms/src/makesms.c"
    cp "$tmp/devkitSMS/SMSlib/src/peep-rules.txt" "$DK/include/"
fi

"$SDCC_DIR/sdcc-4.4.0/bin/sdcc" --version | head -1
echo "toolchain ready:  SDCCBIN=$SDCC_DIR/sdcc-4.4.0/bin   DK=$DK"
