@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set FAIL=0

:: ── Catch2 stdlib tests ──────────────────────────────────────────────────────
echo === Catch2 stdlib tests ===
pushd test
make > "%TEMP%\uhc_catch2.log" 2>&1
if %ERRORLEVEL% neq 0 (
    echo FAIL
    type "%TEMP%\uhc_catch2.log"
    set FAIL=1
) else (
    findstr /C:"All tests passed" /C:"test cases" "%TEMP%\uhc_catch2.log" 2>nul || true
)
popd

:: ── Transpiler feature tests ─────────────────────────────────────────────────
echo.
echo === Transpiler feature tests ===
bash test\transpiler\run.sh
if %ERRORLEVEL% neq 0 set FAIL=1

:: ── Summary ───────────────────────────────────────────────────────────────────
echo.
if %FAIL% equ 0 (
    echo All test suites passed.
    exit /b 0
) else (
    echo One or more test suites failed.
    exit /b 1
)
