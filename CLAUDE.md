# UnholyC (UHC)

A custom programming language that transpiles to C++. The transpiler is a single C++ file.

## File Extensions

- `.uhc` — source files (equivalent to `.cpp`)
- `.uhh` — header files (equivalent to `.hpp`)
- **Never** search for or create `.c`, `.h`, `.cpp`, `.hpp` files for UHC code
- Transpiler outputs `.cc` / `.hh` files (do not edit these)

## Directory Layout

```
transpiler.cpp       Single-file transpiler (~1570 lines, C++17)
uhcstd/              Standard library (io, math, network, std)
graphics/            Vulkan-based graphics library
docgen/              Node.js doc generator for uhclang.org
dist/                Build output — do not edit manually
  bin/               Transpiler binary (unholyc.exe / unholyc)
  include/           Compiled headers (.hh)
  lib/               Compiled libs (libuhc.a, libuhcgraphics.a)
state-machine/       Ignore (experimental, not in active use)
```

## Language Features

### Type System (HolyC-style)

| UHC    | C++ equivalent   |
|--------|------------------|
| `U0`   | `void`           |
| `U8`   | `unsigned char`  |
| `U16`  | `unsigned short` |
| `U32`  | `unsigned int`   |
| `U64`  | `unsigned long long` |
| `I8`   | `char`           |
| `I16`  | `short`          |
| `I32`  | `int`            |
| `I64`  | `long long`      |
| `F32`  | `float`          |
| `F64`  | `double`         |

### Namespaces

Namespaces are the primary organization unit. Dot notation is used for everything:

```
namespace Mutex {
    struct It {
        CRITICAL_SECTION handle
    }
    Mutex.It create()
    U0 lock(Mutex mutex)
}
```

### `self` Keyword

`self` allows using the same name for a struct and its namespace. A `namespace` + `struct It` + `self` pattern means:
- `Mutex` refers to both the namespace and the struct type
- Namespace-self-structs are **references by default** — no need to write `&`

### `lambda` Keyword

Kotlin-style lambdas:

```
lambda myLambda = (I32 x) -> { return x * 2 }
```

### `unused` Keyword

Silences compiler warnings for unused variables. Can appear in a function header or body:

```
U0 foo(unused I32 x) { ... }
// or
unused I32 y = someValue
```

### Other Syntax Notes

- Semicolons are **optional**
- Full C/C++ interop — `#include` of C headers is allowed
- Cross-platform conditionals: `#if defined(_WIN32)` / `#if defined(__linux__)`
- Templates supported: `template<typename T>`

## Build System

After any edit to `transpiler.cpp`, `uhcstd/`, or `graphics/`:

**Windows:**
```
build-all.bat
```

**Linux / macOS:**
```
bash build-all.sh
```

The script: compiles the transpiler → uses the fresh binary to transpile uhcstd + graphics → compiles everything to `.a` libs. With 4k+ lines of UHC going through the transpiler, errors surface immediately.

Use the `/build` slash command to run this from within a Claude Code session.

## Coding Patterns

- Namespace-oriented design — not OOP classes
- Keep structs flat; behavior lives in the namespace, not the struct
- Avoid creating new files unless necessary; prefer extending existing `.uhc` files
- Headers (`.uhh`) declare the public API; implementations go in `.uhc`
