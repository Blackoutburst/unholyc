#!/usr/bin/env bash
set -euo pipefail

UHC=$(command -v unholyc || echo "$HOME/.local/bin/unholyc")
TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
ERRORS=()

run_test() {
    local name="$1"
    local dir="$TESTS_DIR/$name"
    local bin="$dir/_bin"

    printf "%-20s " "$name"

    if ! "$UHC" "$dir" -o "$bin" 2>"$dir/_compile.log"; then
        echo "COMPILE FAIL"
        ERRORS+=("$name: compile failed")
        cat "$dir/_compile.log" >&2
        FAIL=$((FAIL + 1))
        return
    fi

    if ! "$bin" >"$dir/_run.log" 2>&1; then
        echo "RUNTIME FAIL"
        ERRORS+=("$name: runtime failed")
        cat "$dir/_run.log" >&2
        FAIL=$((FAIL + 1))
        rm -f "$bin"
        return
    fi

    echo "PASS"
    PASS=$((PASS + 1))
    rm -f "$bin"
}

run_test self
run_test unused
run_test semicolons
run_test lambda
run_test swizzle
run_test percent_t
run_test operators
run_test templates

echo ""
echo "Results: $PASS passed, $FAIL failed"

if [ ${#ERRORS[@]} -gt 0 ]; then
    echo "Failed tests:"
    for e in "${ERRORS[@]}"; do
        echo "  - $e"
    done
    exit 1
fi
