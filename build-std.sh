#!/bin/bash
set -e

cd "$(dirname "$0")"

OUT="out_std"
DIST="dist"

dist/bin/unholyc uhcstd "$OUT"

INCLUDES=$(find "$OUT" -type d | sed 's|^|-I|' | tr '\n' ' ')

mkdir -p "$OUT/obj"
for f in $(find "$OUT" -name "*.cc"); do
    clang++ -std=c++17 -c $INCLUDES "$f" -o "$OUT/obj/$(basename "${f%.cc}").o"
done

mkdir -p "$DIST/lib"
ar rcs "$DIST/lib/libuhc.a" "$OUT/obj/"*.o

mkdir -p "$DIST/include"
find "$OUT" -name "*.hh" | while read f; do
    cp "$f" "$DIST/include/$(basename "$f")"
done

rm -rf "$OUT"
echo "uhcstd -> $DIST"
