#!/bin/bash
# Downloads what the converters (tools/build_assets.py) read.  They are not part of this project:
#   data/*.WL1   the shareware v1.4 game data (Episode 1), from archive.org
#   wolf3d/      id Software's GPL source release: the names of the graphics and sounds, the 256-colour palette
#   wolf-3d-md/  (only with --genesis) the Genesis port; tools/port_game.py re-derives src/game/*.c from it.
#                The ported files are already in src/game, so building does not need it.
# usage: tools/fetch_data.sh [--genesis] [DIR]
#   DIR defaults to ../work next to the project; the converters look there (tools/paths.py), or in the
#   directories named by WL_DATA, WL_SRC and WL_MD.  The Genesis port goes next to DIR (../wolf-3d-md).
set -euo pipefail
genesis=0
if [ "${1:-}" = "--genesis" ]; then genesis=1; shift; fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
W="${1:-$ROOT/../work}"
mkdir -p "$W/data"
W="$(cd "$W" && pwd)"

if [ -f "$W/data/VSWAP.WL1" ] && [ -f "$W/data/VGAGRAPH.WL1" ]; then
    echo "game data is already in $W/data"
else
    echo "downloading the shareware data ..."
    curl -fL -o "$W/data/wolf3dsw.zip" https://archive.org/download/wolf3dsw/wolf3dsw.zip
    unzip -oq "$W/data/wolf3dsw.zip" -d "$W/data"
fi

if [ -f "$W/wolf3d/WOLFSRC/WL_MAIN.C" ]; then
    echo "id's source release is already in $W/wolf3d"
else
    echo "fetching id's source release ..."
    git clone --depth 1 https://github.com/id-Software/wolf3d "$W/wolf3d"
fi

if [ "$genesis" = 1 ]; then
    if [ -d "$W/../wolf-3d-md/src" ]; then
        echo "the Genesis port is already in $W/../wolf-3d-md"
    else
        git clone --depth 1 https://github.com/haroldo-ok/wolf-3d-md "$W/../wolf-3d-md"
    fi
fi
for f in AUDIOHED AUDIOT GAMEMAPS MAPHEAD VGADICT VGAGRAPH VGAHEAD VSWAP; do
    [ -f "$W/data/$f.WL1" ] || { echo "missing $W/data/$f.WL1" >&2; exit 1; }
done
echo "ready: $W   (tools/paths.py finds it when it is ../work next to the project)"
