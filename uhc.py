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
import hashlib
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
# Lambda transformation
# ---------------------------------------------------------------------------

def _lambda_hash(text: str) -> str:
    """8-char hex hash used to generate unique lambda names."""
    return hashlib.md5(text.encode()).hexdigest()[:8]


def _is_void_type(t: str) -> bool:
    """True when type t is void-like (U0 or void, pointer or not)."""
    return t.strip().rstrip('*').strip() in ('U0', 'void')


def _collect_lambda_signatures(source: str) -> dict:
    """
    Scan for  funcName(... lambda NAME(TYPES) -> RETTYPE ...)
    Returns {func_name: (param_types_str, ret_type_str)}
    """
    result: dict = {}
    pat = re.compile(r'(\w+)\s*\([^(]*\blambda\s+\w+\s*\(([^)]*)\)\s*->\s*(\w+\s*\*?)')
    for m in pat.finditer(source):
        result[m.group(1)] = (m.group(2).strip(), m.group(3).strip())
    return result


def _transform_lambda_decls(source: str) -> str:
    """Replace  lambda NAME(TYPES) -> RETTYPE  with  RETTYPE (*NAME)(TYPES)."""
    def repl(m: re.Match) -> str:
        name = m.group(1).strip()
        types = m.group(2).strip()
        ret = m.group(3).strip()
        return f'{ret} (*{name})({types})'
    return re.sub(r'\blambda\s+(\w+)\s*\(([^)]*)\)\s*->\s*(\w+\s*\*?)', repl, source)


# Matches a lambda call-site opening line:
#   [indent][TYPE name = ]funcName(args) { (params) ->
#   [indent][TYPE name = ]Namespace.funcName(args) { (params) ->
_CALLSITE_PAT = re.compile(
    r'^(\s*)'                               # group 1: leading indent
    r'((?:\w[\w\s\*]*\s+)?\w+\s*=\s*)?'   # group 2: optional lvalue  "I32 result = "
    r'(\w+(?:\.\w+)*)'                      # group 3: function name (possibly qualified)
    r'\s*\(([^)]*)\)'                       # group 4: function args
    r'\s*\{\s*\(([^)]*)\)\s*->\s*$'        # group 5: lambda params after {
)

# Matches a no-param lambda call-site:
#   [indent][TYPE name = ]funcName(args) {
# Only used when funcName is known to accept a lambda (checked against sig_map).
_CALLSITE_PAT_NOPARAM = re.compile(
    r'^(\s*)'                               # group 1: leading indent
    r'((?:\w[\w\s\*]*\s+)?\w+\s*=\s*)?'   # group 2: optional lvalue
    r'(\w+(?:\.\w+)*)'                      # group 3: function name (possibly qualified)
    r'\s*\(([^)]*)\)'                       # group 4: function args
    r'\s*\{\s*$'                            # just {, end of line
)

# Matches a function *definition* line (ends with {, not ;)
# Excludes control-flow keywords.
_FUNCDEF_PAT = re.compile(
    r'^\s*(?!(?:if|else|for|while|switch|do)\b)\w.*\)\s*\{\s*$'
)


def _transform_one_lambda(lines: list, sig_map: dict) -> tuple:
    """
    Find and transform the first lambda call site in `lines`.
    Returns (new_lines, changed_bool).
    """
    for start_idx, line in enumerate(lines):
        m = _CALLSITE_PAT.match(line)
        noparam = False
        if not m:
            m2 = _CALLSITE_PAT_NOPARAM.match(line)
            if m2 and (m2.group(3) in sig_map or m2.group(3).split('.')[-1] in sig_map):
                m = m2
                noparam = True
        if not m:
            continue

        indent        = m.group(1)
        lvalue        = m.group(2)          # e.g. "I32 result = " or None
        func_name     = m.group(3)
        func_args     = m.group(4).strip()
        lambda_params = '' if noparam else m.group(5).strip()  # e.g. "a, b"

        # --- Collect body lines until matching } ---
        depth = 1
        body: list = []
        j = start_idx + 1
        while j < len(lines) and depth > 0:
            l = lines[j]
            depth += l.count('{') - l.count('}')
            if depth > 0:
                body.append(l)
            j += 1
        close_idx = j - 1

        # --- Signature lookup (try qualified name, then unqualified) ---
        sig = sig_map.get(func_name) or sig_map.get(func_name.split('.')[-1])
        if sig:
            ptypes_str, ret_type = sig
            types = [t.strip() for t in ptypes_str.split(',') if t.strip()]
            names = [n.strip() for n in lambda_params.split(',') if n.strip()]
            typed_params = ', '.join(f'{t} {n}' for t, n in zip(types, names))
        else:
            typed_params = lambda_params
            ret_type = 'U0'

        # --- Inject return before last statement if return type is non-void ---
        if not _is_void_type(ret_type) and body:
            if not any(re.search(r'\breturn\b', bl) for bl in body):
                for bi in range(len(body) - 1, -1, -1):
                    s = body[bi].strip()
                    if s and not s.startswith('//'):
                        lead = re.match(r'^(\s*)', body[bi]).group(1)
                        body[bi] = f'{lead}return {s}'
                        break

        # --- Generate unique lambda name ---
        lname = f'__lambda_{_lambda_hash(func_name + lambda_params + chr(10).join(body))}'

        # --- Build lambda function lines ---
        lambda_lines = [f'{ret_type} {lname}({typed_params}) {{', *body, '}', '']

        # --- Find hoist point: before the enclosing function definition ---
        hoist_idx = 0
        for k in range(start_idx - 1, -1, -1):
            if _FUNCDEF_PAT.match(lines[k]):
                hoist_idx = k
                break

        # --- Build replacement call line ---
        new_args = f'{func_args}, {lname}' if func_args else lname
        if lvalue:
            call = f'{indent}{lvalue}{func_name}({new_args});'
        else:
            call = f'{indent}{func_name}({new_args});'

        # --- Assemble ---
        new_lines = (
            lines[:hoist_idx]
            + lambda_lines
            + lines[hoist_idx:start_idx]
            + [call]
            + lines[close_idx + 1:]
        )
        return new_lines, True

    return lines, False


def _transform_lambdas(source: str, extra_sig_map: dict = None) -> str:
    """
    Full lambda pipeline:
      1. Collect signatures (for return-type injection at call sites).
      2. Transform lambda parameter declarations  ->  function pointer syntax.
      3. Transform call sites (hoist generated functions, rewrite call).
    """
    source = source.replace('\r\n', '\n').replace('\r', '\n')
    sig_map = _collect_lambda_signatures(source)
    if extra_sig_map:
        for k, v in extra_sig_map.items():
            if k not in sig_map:
                sig_map[k] = v
    source = _transform_lambda_decls(source)
    lines = source.split('\n')
    changed = True
    while changed:
        lines, changed = _transform_one_lambda(lines, sig_map)
    return '\n'.join(lines)


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
            # but only if the member name is not snake_case (no underscores),
            # which would indicate a C struct field rather than a namespace member.
            if (j < n and line[j] == '.'
                    and j + 1 < n
                    and (line[j + 1].isalpha() or line[j + 1] == '_')):
                k = j + 1
                while k < n and (line[k].isalnum() or line[k] == '_'):
                    k += 1
                member = line[j + 1:k]
                if '_' not in member or member[0].isupper():
                    result.append(ident + '::')
                    i = j + 1          # skip the dot
                    continue

            result.append(ident)
            i = j
            continue

        result.append(ch)
        i += 1

    return ''.join(result)


def _replace_unused(line: str) -> str:
    """
    Replace 'unused' keyword:
      unused foo;          ->  UNUSED_VAR(foo);              (statement in body)
      unused TYPE VARNAME  ->  TYPE VARNAME __attribute__((unused))  (parameter/declaration)
    Skips // line comments.
    """
    if 'unused' not in line:
        return line

    comment_pos = line.find('//')
    if comment_pos == 0:
        return line
    code_part, comment_part = (line[:comment_pos], line[comment_pos:]) if comment_pos > 0 else (line, '')

    # Statement form first (has semicolon)
    code_part = re.sub(r'\bunused\s+(\w+)\s*;', r'UNUSED_VAR(\1);', code_part)
    # Parameter / declaration form: unused TYPE VARNAME
    code_part = re.sub(r'\bunused\s+(\w+)\s+(\w+)\b', r'\1 \2 __attribute__((unused))', code_part)

    return code_part + comment_part


def transpile(source: str, extra_sig_map: dict = None) -> str:
    """Return the transpiled C++ source for a given UnHolyC source string."""
    # Pre-process: lambda syntax must run before line-by-line passes
    source = _transform_lambdas(source, extra_sig_map)

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

        # 2. Replace 'unused' keyword
        line = _replace_unused(line)

        # 3. Namespace dot -> double-colon
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
                   verbose: bool = True,
                   extra_sig_map: dict = None) -> str:
    """Transpile one file and return the directory it was written to."""
    with open(input_path, 'r', encoding='utf-8') as fh:
        source = fh.read()

    output = transpile(source, extra_sig_map)
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

    # First pass: collect lambda signatures from all files so cross-file
    # call sites (e.g. functions declared in a header) get correct types.
    global_sig_map: dict = {}
    for f in files:
        with open(f, 'r', encoding='utf-8') as fh:
            global_sig_map.update(_collect_lambda_signatures(fh.read()))

    dest_dirs: set[str] = set()
    for f in files:
        dest_dirs.add(transpile_file(f, args.out_dir, extra_sig_map=global_sig_map))

    # If an output root was given, types.hh goes into out/include/.
    # Otherwise it goes next to each group of output files.
    if args.out_dir:
        write_types_hh({os.path.join(os.path.abspath(args.out_dir), 'include')})
    else:
        write_types_hh(dest_dirs)
    print('Done.')


if __name__ == '__main__':
    main()
