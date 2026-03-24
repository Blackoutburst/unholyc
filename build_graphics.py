#!/usr/bin/env python3
"""
Build the UnHolyC graphics module (graphics/) into a static library.

Output
------
  graphics/dist/
    include/
      uhcgraphics.hh     <- single entry-point header
      core/              <- internal headers preserved for transitive includes
      debug/
      window/
      utils/
    lib/
      libuhcgraphics.a

Usage in any project
--------------------
  clang++ your_files.cc
    -I<uhcstd>/dist/include  -I<graphics>/dist/include
    -L<uhcstd>/dist/lib      -lunholyc
    -L<graphics>/dist/lib    -luhcgraphics
    -lvulkan -lglfw3
    -o myapp
"""

import os
import sys
import glob
import shutil
import subprocess
import tempfile
import argparse

ROOT     = os.path.dirname(os.path.abspath(__file__))
GRAPHICS = os.path.join(ROOT, 'graphics')
DIST_DIR = os.path.join(GRAPHICS, 'dist')
UHC_DIST = os.path.join(ROOT, 'dist')


# ── Helpers ────────────────────────────────────────────────────────────────────

def run(cmd: list[str]) -> None:
    print('  $', ' '.join(cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        print(f'\nError: command exited with code {r.returncode}', file=sys.stderr)
        sys.exit(r.returncode)


def find(candidates: list[str]) -> str | None:
    return next((c for c in candidates if shutil.which(c)), None)


# ── Main ───────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description='Build UnHolyC graphics module')
    parser.add_argument('--graphics', default=GRAPHICS,                 help='graphics source dir')
    parser.add_argument('--out',      default=DIST_DIR,                 help='output dist dir')
    parser.add_argument('--uhc-dist', default=UHC_DIST,                 help='uhcstd dist dir')
    parser.add_argument('--vk-inc',   default='/opt/homebrew/include',  help='Vulkan include path')
    parser.add_argument('--glfw-inc', default='/opt/homebrew/include',  help='GLFW include path')
    parser.add_argument('--cppstd',   default='c++17',                  help='C++ standard')
    args = parser.parse_args()

    clangpp  = find(['clang++', 'clang++-18', 'clang++-17', 'clang++-16', 'clang++-15'])
    archiver = find(['llvm-ar', 'ar'])

    if not clangpp:  sys.exit('Error: clang++ not found in PATH')
    if not archiver: sys.exit('Error: llvm-ar / ar not found in PATH')

    print(f'clang++ : {shutil.which(clangpp)}')
    print(f'archiver: {shutil.which(archiver)}')

    with tempfile.TemporaryDirectory() as tmp:

        # ── 1. Transpile ──────────────────────────────────────────────────────
        print('\n-- Transpiling ----------------------------------------------')
        run(['unholyc', args.graphics, '-r', '-o', tmp])

        gfx_name = os.path.basename(os.path.normpath(args.graphics))
        src_dir  = os.path.join(tmp, gfx_name)
        inc_tmp  = os.path.join(tmp, 'include')          # types.hh lives here
        inc_src  = os.path.join(src_dir, 'include')      # graphics headers

        # Copy plain .h files the transpiler ignores
        for h in glob.glob(os.path.join(args.graphics, '**', '*.h'), recursive=True):
            rel    = os.path.relpath(h, args.graphics)
            dest_h = os.path.join(src_dir, rel)
            os.makedirs(os.path.dirname(dest_h), exist_ok=True)
            shutil.copy2(h, dest_h)

        # ── 2. Compile → object files (exclude main.cc) ───────────────────────
        print('\n-- Compiling ------------------------------------------------')
        cc_files = glob.glob(os.path.join(src_dir, '**', '*.cc'), recursive=True)
        cc_files = [f for f in cc_files if os.path.basename(f) != 'main.cc']

        if not cc_files:
            sys.exit('Error: no .cc files produced by transpiler')

        include_flags = [
            '-I', inc_tmp,
            '-I', inc_src,
            '-I', os.path.join(args.uhc_dist, 'include'),
            '-isystem', args.vk_inc,
            '-I', args.glfw_inc,
        ]

        obj_files: list[str] = []
        for cc in cc_files:
            obj = cc + '.o'
            run([clangpp, '-c', cc, '-o', obj,
                 f'-std={args.cppstd}', '-Wno-writable-strings'] + include_flags)
            obj_files.append(obj)

        # ── 3. Archive → static library ───────────────────────────────────────
        print('\n-- Archiving ------------------------------------------------')
        lib_path = os.path.join(tmp, 'libuhcgraphics.a')
        run([archiver, 'rcs', lib_path] + obj_files)

        # ── 4. Install to dist/ ───────────────────────────────────────────────
        print('\n-- Installing -----------------------------------------------')
        inc_out = os.path.join(args.out, 'include')
        lib_out = os.path.join(args.out, 'lib')
        os.makedirs(lib_out, exist_ok=True)

        # Headers — preserve subdirectory structure (core/, window/, debug/, utils/)
        for hh in glob.glob(os.path.join(inc_src, '**', '*.hh'), recursive=True):
            rel  = os.path.relpath(hh, inc_src)
            dest = os.path.join(inc_out, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(hh, dest)
            print(f'  {rel}')

        for h in glob.glob(os.path.join(inc_src, '**', '*.h'), recursive=True):
            rel  = os.path.relpath(h, inc_src)
            dest = os.path.join(inc_out, rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy2(h, dest)
            print(f'  {rel}')

        # Static library
        dest_lib = os.path.join(lib_out, 'libuhcgraphics.a')
        shutil.copy2(lib_path, dest_lib)
        print(f'  libuhcgraphics.a  ->  {lib_out}')

    # ── Summary ───────────────────────────────────────────────────────────────
    uhc_inc = os.path.join(args.uhc_dist, 'include')
    uhc_lib = os.path.join(args.uhc_dist, 'lib')
    print(f"""
-- Done -----------------------------------------------------
  {inc_out}
  {dest_lib}

To use in any project:
  clang++ your_files.cc \\
    -I{uhc_inc} -I{inc_out} \\
    -L{uhc_lib} -lunholyc \\
    -L{lib_out} -luhcgraphics \\
    -lvulkan -lglfw3 \\
    -o myapp
""")


if __name__ == '__main__':
    main()
