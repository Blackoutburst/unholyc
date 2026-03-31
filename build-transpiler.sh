#!/bin/bash
set -e

cd "$(dirname "$0")"

mkdir -p dist/bin

echo "Compiling transpiler..."
c++ -std=c++17 -O2 -Wall -Wextra -Werror -o dist/bin/unholyc transpiler.cpp

echo "unholyc -> dist/bin/unholyc"
