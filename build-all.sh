#!/bin/bash
set -e

cd "$(dirname "$0")"

bash build-transpiler.sh
bash build-std.sh
bash build-graphics.sh
