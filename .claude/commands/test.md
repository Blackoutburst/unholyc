Run the full UnholyC test suite: Catch2 stdlib tests + transpiler feature tests.

Steps:
1. From the repo root, run the Catch2 stdlib tests:
   ```
   cd test && make
   ```
   This compiles and runs all test_*.cpp files. Look for "All tests passed" or capture any failures.

2. From the repo root, run the transpiler feature tests:
   ```
   bash test/transpiler/run.sh
   ```
   This compiles and runs each test/transpiler/<name>/main.uhc via `unholyc`. Look for "Results: N passed, 0 failed".

3. Report a combined summary: total Catch2 assertions, total transpiler tests, and any failures with their error output.

Context: Catch2 tests verify the compiled stdlib (uhcstd) via C++ headers. Transpiler tests verify the transpiler itself by writing UHC source, compiling it, and running the binary. Both suites must pass before committing.
