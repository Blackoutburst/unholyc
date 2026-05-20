#!/bin/bash
set -e

cd "$(dirname "$0")"

mkdir -p dist/bin

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "dev")

echo "Compiling transpiler..."
c++ -std=c++17 -O2 -Wall -Wextra -Werror -DUHC_COMMIT=$COMMIT -o dist/bin/unholyc transpiler.cpp

echo "unholyc -> dist/bin/unholyc"
