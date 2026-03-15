# UnHolyC

A small C dialect that adds **HolyC types** and **dot namespace syntax** on top of C++.
Files use `.uhc` (source) and `.uhh` (header) extensions and transpile to plain `.cc` / `.hh`.

## Language features

### HolyC types

| Type   | C equivalent     |
|--------|------------------|
| `U0`   | `void`           |
| `U8`   | `uint8_t`        |
| `U16`  | `uint16_t`       |
| `U32`  | `uint32_t`       |
| `U64`  | `uint64_t`       |
| `I8`   | `char`           |
| `I16`  | `int16_t`        |
| `I32`  | `int32_t`        |
| `I64`  | `int64_t`        |
| `F32`  | `float`          |
| `F64`  | `double`         |
| `F128` | `long double`    |

These are automatically available in every `.uhc` / `.uhh` file — no manual include needed.

### Dot namespace syntax

Use `.` instead of `::` to access namespaces:

```c
// UnHolyC (.uhc / .uhh)
namespace Pair {
    struct It { I32 a; I32 b; };
    U0 set(Pair.It& pair, I32 a, I32 b);
}

Pair.It p = { 0, 0 };
Pair.set(p, 5, 4);
```

```cpp
// Transpiled output (.cc / .hh)
namespace Pair {
    struct It { I32 a; I32 b; };
    U0 set(Pair::It& pair, I32 a, I32 b);
}

Pair::It p = { 0, 0 };
Pair::set(p, 5, 4);
```

**Rule:** an identifier starting with an **uppercase letter** followed by `.` is treated as a
namespace qualifier. Lowercase variable member access (`pair.a`, `desc.stride`) is left untouched.

---

## Transpiler

### Requirements

- Python 3.10+
- `pyinstaller` (only needed to build the executable)

```bash
pip install pyinstaller
```

### Build

#### Windows

```bash
pyinstaller --onefile --console --name unholyc uhc.py
copy dist\unholyc.exe C:\Users\<you>\.local\bin\unholyc.exe
```

#### macOS / Linux

```bash
pyinstaller --onefile --console --name unholyc uhc.py
cp dist/unholyc /usr/local/bin/unholyc
chmod +x /usr/local/bin/unholyc
```

> After copying, delete the `build/`, `dist/` folders and `unholyc.spec` — they are not needed.

---

## Usage

```
unholyc <inputs> [-o <out_dir>] [-r]

  inputs        .uhc / .uhh files or directories
  -o <out_dir>  output directory (default: write alongside source)
  -r            recurse into subdirectories
```

### Typical project layout

```
project/
  src/          ← .uhc source files
  include/      ← .uhh header files
```

### Transpile and build

```bash
# 1. Transpile everything into out/
unholyc ./ -r -o out

# Output structure:
#   out/src/*.cc
#   out/include/*.hh
#   out/include/types.hh   ← generated automatically

# 2. Compile with clang++ (or g++)
clang++ out/src/*.cc -Iout/include -o myapp
```

### Transpile a single file

```bash
unholyc src/main.uhc include/pair.uhh -o out
```

### Transpile without a separate output directory

Files are written alongside the source with `.cc` / `.hh` extensions:

```bash
unholyc src/main.uhc
# produces src/main.cc
```
