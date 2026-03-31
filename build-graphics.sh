#!/bin/bash
set -e

cd "$(dirname "$0")"

OUT="out_graphics"
DIST="dist"

readonly I_UHC="$DIST/include"
readonly I_VK="/opt/homebrew/include"
readonly I_GLFW="/opt/homebrew/include"

dist/bin/unholyc -I"$I_UHC" graphics "$OUT"

mkdir -p "$OUT/obj"
for f in $(find "$OUT/src" -name "*.cc" ! -name "main.cc"); do
    clang++ -std=c++17 -c \
        -I"$OUT/include" \
        -isystem "$I_UHC" \
        -isystem "$I_VK" \
        -isystem "$I_GLFW" \
        "$f" -o "$OUT/obj/$(basename "${f%.cc}").o"
done

mkdir -p "$DIST/lib"
ar rcs "$DIST/lib/libuhcgraphics.a" "$OUT/obj/"*.o

mkdir -p "$DIST/include"
cp -r "$OUT/include/." "$DIST/include/"

rm -rf "$OUT"
echo "graphics -> $DIST"
