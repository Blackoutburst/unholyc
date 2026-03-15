#!/usr/bin/env python3
"""
UnHolyC transpiler  —  .uhc / .uhh  ->  .cc / .hh

Features
--------
  1. HolyC types auto-included (U0, U8, U16, U32, U64,
                                 I8, I16, I32, I64,
                                 F32, F64, F128)
  2. Dot namespace access:  Cube.create()  ->  Cube::create()
     Rule: UppercaseName.<alpha/underscore>  is treated as a
     namespace qualifier.  Lowercase variables (cube.field,
     desc.attr) are left alone — they are plain C struct members.

Usage
-----
  python uhc.py file.uhc [file2.uhh ...]   # transpile files
  python uhc.py src/ -r                    # recurse into a directory
  python uhc.py src/ -r -o build/          # write outputs to build/
"""

import re
import sys
import os
import argparse
from typing import Optional

TYPES_INCLUDE = '#include "types.hh"'

TYPES_HH_CONTENT = """\
#pragma once
#include <stdint.h>

typedef void          U0;

typedef uint64_t      U64;
typedef uint32_t      U32;
typedef uint16_t      U16;
typedef unsigned char U8;

typedef int64_t       I64;
typedef int32_t       I32;
typedef int16_t       I16;
typedef char          I8;

typedef float         F32;
typedef double        F64;
typedef long double   F128;

#define UNUSED_VAR(expr) do { (void)(expr); } while (0)
"""


# ---------------------------------------------------------------------------
# Core transformation helpers
# ---------------------------------------------------------------------------

def _replace_namespace_dots(line: str) -> str:
    """
    Replace  UppercaseName.identifier  with  UppercaseName::identifier
    inside a single source line, skipping string literals and // comments.
    """
    result: list[str] = []
    i = 0
    n = len(line)

    while i < n:
        ch = line[i]

        # -- Line comment: copy the rest verbatim
        if ch == '/' and i + 1 < n and line[i + 1] == '/':
            result.append(line[i:])
            break

        # -- String / char literals: copy verbatim
        if ch in ('"', "'"):
            quote = ch
            result.append(ch)
            i += 1
            while i < n:
                c = line[i]
                result.append(c)
                if c == '\\':          # escaped character
                    i += 1
                    if i < n:
                        result.append(line[i])
                elif c == quote:
                    break
                i += 1
            i += 1
            continue

        # -- Uppercase identifier: possible namespace qualifier
        if ch.isupper():
            j = i
            while j < n and (line[j].isalnum() or line[j] == '_'):
                j += 1
            ident = line[i:j]

            # Followed by '.' and then alpha/underscore  ->  namespace access
            if (j < n and line[j] == '.'
                    and j + 1 < n
                    and (line[j + 1].isalpha() or line[j + 1] == '_')):
                result.append(ident + '::')
                i = j + 1          # skip the dot
                continue

            result.append(ident)
            i = j
            continue

        result.append(ch)
        i += 1

    return ''.join(result)


def transpile(source: str) -> str:
    """Return the transpiled C++ source for a given UnHolyC source string."""
    lines = source.splitlines(keepends=True)
    result: list[str] = []
    has_types_include = False
    pragma_once_idx = -1

    for i, raw_line in enumerate(lines):
        line = raw_line

        # Track whether types.hh is already included
        stripped = line.strip()
        if 'types.hh' in stripped or 'types.uhh' in stripped:
            has_types_include = True

        if stripped == '#pragma once':
            pragma_once_idx = i

        # 1. Fix #include extensions:  .uhh -> .hh,  .uhc -> .cc
        line = re.sub(r'(#\s*include\s+"[^"]+)\.uhh(")', r'\1.hh\2', line)
        line = re.sub(r'(#\s*include\s+"[^"]+)\.uhc(")', r'\1.cc\2', line)
        line = re.sub(r'(#\s*include\s+<[^>]+)\.uhh(>)', r'\1.hh\2', line)
        line = re.sub(r'(#\s*include\s+<[^>]+)\.uhc(>)', r'\1.cc\2', line)

        # 2. Namespace dot -> double-colon
        line = _replace_namespace_dots(line)

        result.append(line)

    # 3. Auto-inject types include if not present
    if not has_types_include:
        inject = TYPES_INCLUDE + '\n'
        if pragma_once_idx >= 0:
            result.insert(pragma_once_idx + 1, inject)   # right after #pragma once
        else:
            result.insert(0, inject)

    return ''.join(result)


# ---------------------------------------------------------------------------
# File I/O
# ---------------------------------------------------------------------------

def _output_path(input_path: str, out_dir: Optional[str]) -> str:
    """Derive the output .cc/.hh path from a .uhc/.uhh input path."""
    base, ext = os.path.splitext(input_path)
    new_ext = {'.uhc': '.cc', '.uhh': '.hh'}.get(ext, ext)

    if out_dir:
        # Mirror the relative structure inside out_dir
        rel = os.path.relpath(base + new_ext)
        return os.path.join(out_dir, rel)

    return base + new_ext


def write_types_hh(dest_dirs: set[str], verbose: bool = True) -> None:
    """Write types.hh into every directory that will contain output files."""
    for d in dest_dirs:
        os.makedirs(d, exist_ok=True)
        dest = os.path.join(d, 'types.hh')
        with open(dest, 'w', encoding='utf-8') as fh:
            fh.write(TYPES_HH_CONTENT)
        if verbose:
            print(f'  (generated)  ->  {dest}')


def transpile_file(input_path: str,
                   out_dir: Optional[str] = None,
                   verbose: bool = True) -> str:
    """Transpile one file and return the directory it was written to."""
    with open(input_path, 'r', encoding='utf-8') as fh:
        source = fh.read()

    output = transpile(source)
    dest = _output_path(input_path, out_dir)

    parent = os.path.dirname(os.path.abspath(dest))
    os.makedirs(parent, exist_ok=True)

    with open(dest, 'w', encoding='utf-8') as fh:
        fh.write(output)

    if verbose:
        print(f'  {input_path}  ->  {dest}')

    return parent


def collect_files(inputs: list[str], recursive: bool) -> list[str]:
    files: list[str] = []
    for inp in inputs:
        if os.path.isdir(inp):
            if recursive:
                for root, _dirs, fnames in os.walk(inp):
                    for fn in fnames:
                        if fn.endswith(('.uhc', '.uhh')):
                            files.append(os.path.join(root, fn))
            else:
                for fn in os.listdir(inp):
                    if fn.endswith(('.uhc', '.uhh')):
                        files.append(os.path.join(inp, fn))
        elif os.path.isfile(inp):
            files.append(inp)
        else:
            print(f'Warning: {inp!r} not found, skipping.', file=sys.stderr)
    return files


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        prog='uhc',
        description='UnHolyC transpiler: .uhc/.uhh  ->  .cc/.hh',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        'inputs', nargs='+',
        help='.uhc / .uhh files or directories',
    )
    parser.add_argument(
        '-o', '--out-dir',
        help='Output directory (default: write alongside source files)',
    )
    parser.add_argument(
        '-r', '--recursive', action='store_true',
        help='Recurse into directories',
    )
    args = parser.parse_args()

    files = collect_files(args.inputs, args.recursive)
    if not files:
        print('No .uhc/.uhh files found.', file=sys.stderr)
        sys.exit(1)

    print(f'Transpiling {len(files)} file(s)...')
    dest_dirs: set[str] = set()
    for f in files:
        dest_dirs.add(transpile_file(f, args.out_dir))

    # If an output root was given, types.hh goes into out/include/.
    # Otherwise it goes next to each group of output files.
    if args.out_dir:
        write_types_hh({os.path.join(os.path.abspath(args.out_dir), 'include')})
    else:
        write_types_hh(dest_dirs)
    print('Done.')


if __name__ == '__main__':
    main()
