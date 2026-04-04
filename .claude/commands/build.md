Run the UnholyC full build to compile the transpiler and validate uhcstd + graphics.

Steps:
1. Detect the platform: if on Windows (`$OSTYPE` is not set or `uname` returns `MINGW`/`MSYS`/similar, or running under cmd/PowerShell) run `build-all.bat`; otherwise run `bash build-all.sh`
2. Run the script from the repo root (the directory containing `transpiler.cpp`)
3. Stream or capture the output. On success, confirm the build passed. On failure, show the relevant error lines so we can diagnose and fix immediately.

Context: this script recompiles the transpiler from `transpiler.cpp`, then uses the fresh binary to transpile all of `uhcstd/` and `graphics/` (~4k lines of UHC). Any regression in the transpiler will surface as a compile or transpile error here — treat this as the integration test suite.
