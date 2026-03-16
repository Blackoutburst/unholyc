#!/usr/bin/env python3
"""
Build the UnHolyC standard library (uhcstd/) into a static library.

Output
------
  dist/
    include/        <- copy this to your project or add -Idist/include
      types.hh
      unholyio.hh
      ...
    lib/
      libunholyc.a  <- link with -Ldist/lib -lunholyc

Usage in any project
--------------------
  clang++ your_files.cc -Idist/include -Ldist/lib -lunholyc -o myapp
"""

import os
import sys
import glob
import shutil
import subprocess
import tempfile
import platform
import argparse

ROOT     = os.path.dirname(os.path.abspath(__file__))
UHCSTD   = os.path.join(ROOT, 'uhcstd')
DIST_DIR = os.path.join(ROOT, 'dist')


# ── Helpers ───────────────────────────────────────────────────────────────────

def run(cmd: list[str]) -> None:
    print('  $', ' '.join(cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print(f'\nError: command exited with code {r.returncode}', file=sys.stderr)
        sys.exit(r.returncode)


def find(candidates: list[str]) -> str | None:
    return next((c for c in candidates if shutil.which(c)), None)


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description='Build UnHolyC stdlib')
    parser.add_argument('--std',     default=UHCSTD,   help='uhcstd source dir')
    parser.add_argument('--out',     default=DIST_DIR,  help='output dist dir')
    parser.add_argument('--cppstd',  default='c++17',   help='C++ standard (default: c++17)')
    args = parser.parse_args()

    # ── Find tools ────────────────────────────────────────────────────────────
    clangpp = find(['clang++', 'clang++-18', 'clang++-17', 'clang++-16', 'clang++-15'])
    if not clangpp:
        sys.exit('Error: clang++ not found in PATH')

    archiver = find(['llvm-ar', 'ar', 'llvm-ar-18', 'llvm-ar-17', 'llvm-ar-16'])
    if not archiver:
        sys.exit('Error: llvm-ar / ar not found in PATH')

    print(f'clang++ : {shutil.which(clangpp)}')
    print(f'archiver: {shutil.which(archiver)}')

    with tempfile.TemporaryDirectory() as tmp:

        # ── 1. Transpile ──────────────────────────────────────────────────────
        print('\n-- Transpiling ----------------------------------------------')
        run(['unholyc', args.std, '-r', '-o', tmp])

        # After transpile the layout is:
        #   tmp/<uhcstd_basename>/*.cc  *.hh
        #   tmp/include/types.hh
        uhcstd_name = os.path.basename(os.path.normpath(args.std))
        src_dir     = os.path.join(tmp, uhcstd_name)
        inc_tmp     = os.path.join(tmp, 'include')          # types.hh lives here

        cc_files = glob.glob(os.path.join(src_dir, '**', '*.cc'), recursive=True)
        if not cc_files:
            sys.exit('Error: no .cc files produced by transpiler')

        include_flags = ['-I', inc_tmp, '-I', src_dir]
        libs = [] if platform.system() == 'Windows' else ['-pthread']

        # ── 2. Compile → object files ─────────────────────────────────────────
        print('\n-- Compiling ------------------------------------------------')
        obj_files: list[str] = []
        for cc in cc_files:
            obj = cc + '.o'
            run([clangpp, '-c', cc, '-o', obj, f'-std={args.cppstd}'] + include_flags + libs)
            obj_files.append(obj)

        # ── 3. Archive → static library ───────────────────────────────────────
        print('\n-- Archiving ------------------------------------------------')
        lib_path = os.path.join(tmp, 'libunholyc.a')
        run([archiver, 'rcs', lib_path] + obj_files)

        # ── 4. Install to dist/ ───────────────────────────────────────────────
        print('\n-- Installing -----------------------------------------------')
        inc_out = os.path.join(args.out, 'include')
        lib_out = os.path.join(args.out, 'lib')
        os.makedirs(inc_out, exist_ok=True)
        os.makedirs(lib_out, exist_ok=True)

        # types.hh
        shutil.copy2(os.path.join(inc_tmp, 'types.hh'), inc_out)
        print(f'  types.hh  ->  {inc_out}')

        # all stdlib headers
        for hh in glob.glob(os.path.join(src_dir, '**', '*.hh'), recursive=True):
            shutil.copy2(hh, inc_out)
            print(f'  {os.path.basename(hh)}  ->  {inc_out}')

        # static lib
        dest_lib = os.path.join(lib_out, 'libunholyc.a')
        shutil.copy2(lib_path, dest_lib)
        print(f'  libunholyc.a  ->  {lib_out}')

    # ── Summary ───────────────────────────────────────────────────────────────
    print(f"""
-- Done -----------------------------------------------------
  {inc_out}
  {dest_lib}

To use in any project:
  clang++ your_files.cc -I{inc_out} -L{lib_out} -lunholyc -o myapp
""")


if __name__ == '__main__':
    main()
