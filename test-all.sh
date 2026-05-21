#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

PASS=0
FAIL=0

# ── Catch2 stdlib tests ───────────────────────────────────────────────────────
echo "=== Catch2 stdlib tests ==="
if make -C test > /tmp/uhc_catch2.log 2>&1; then
    grep -E "All tests passed|test cases" /tmp/uhc_catch2.log || true
    PASS=$((PASS + 1))
else
    echo "FAIL"
    cat /tmp/uhc_catch2.log
    FAIL=$((FAIL + 1))
fi

# ── Transpiler feature tests ─────────────────────────────────────────────────
echo ""
echo "=== Transpiler feature tests ==="
if bash test/transpiler/run.sh; then
    PASS=$((PASS + 1))
else
    FAIL=$((FAIL + 1))
fi

# ── Summary ───────────────────────────────────────────────────────────────────
echo ""
if [ $FAIL -eq 0 ]; then
    echo "All test suites passed."
    exit 0
else
    echo "$FAIL suite(s) failed."
    exit 1
fi
