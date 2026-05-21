# UnholyC (UHC)

A custom programming language that transpiles to C++. The transpiler is a single C++ file.

## File Extensions

- `.uhc` — source files (equivalent to `.cpp`)
- `.uhh` — header files (equivalent to `.hpp`)
- **Never** search for or create `.c`, `.h`, `.cpp`, `.hpp` files for UHC code
- Transpiler outputs `.cc` / `.hh` files (do not edit these)

## Directory Layout

```
transpiler.cpp       Single-file transpiler (~2450 lines, C++17)
uhcstd/              Standard library (io, math, network, std)
graphics/            Vulkan-based graphics library
examples/            Runnable examples covering language + stdlib features
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
namespace Counter {
    self {
        I32 value = 0
    }
    Counter create()
    U0 increment(Counter c)
}
```

### `self` Keyword

`self` replaces `struct It` — it declares the struct type for the namespace and makes the namespace name serve as both namespace and type:

```
namespace Mutex {
    self {
        pthread_mutex_t handle
    }
    Mutex create()       // returns Mutex, not Mutex.It
    U0 lock(Mutex m)     // m is passed by reference automatically
}
```

- `Mutex` is both the namespace and the struct type
- Namespace-self-structs are **references by default** when passed as function parameters — no need to write `&`

### `lambda` Keyword

Kotlin-style trailing lambdas. Two supported forms:

**Lambda parameter in function signature:**
```
U0 forEach(I32* arr, I32 len, lambda block(I32) -> U0) {
    for (I32 i = 0; i < len; i++) { block(arr[i]) }
}
```

Template types in lambda params are fully supported — the transpiler injects a `typename __Block_<name>` into the `template<>` declaration automatically:
```
template<typename T>
U0 forEach(List<T> list, lambda block(T) -> U0) { ... }
```

**Trailing lambda call syntax:**
```
forEach(nums, 5) { (n) ->
    Log.info("%d", n)
}
```

Works anywhere — top-level, inside namespaces, and cross-file. The transpiler pre-scans `.uhh` source headers and compiled `.hh` headers to register lambda-accepting functions before transpiling call sites.

If the lambda body captures local variables, the transpiler emits an inline C++ lambda with a capture list. Otherwise it hoists to a named generic lambda object (`auto __lambda_xxx = [](auto param) { ... }`).

Stored lambdas (`lambda x = ...`) and lambdas with non-void return types are not supported.

### `%T` Format Specifier

`%T` in any format string (Log calls, etc.) is rewritten to `%s` and the corresponding argument is wrapped in `uhc_tostring(...)`. Works both in regular code and inside lambda bodies.

The transpiler auto-generates a `uhc_tostring` specialization for every namespace that defines a `toString` method. Namespaces without `toString` fall back to `"[object]"`.

```
namespace Color {
    self { U8 r; U8 g; U8 b }
    const I8* toString(Color c) {
        return "rgb(%d, %d, %d)", c.r, c.g, c.b
    }
}

Color c = {255, 128, 0}
Log.info("%T", c)  // → [INFO] rgb(255, 128, 0)
```

### `unused` Keyword

Silences compiler warnings for unused variables. Can appear in a function header or body:

```
U0 foo(unused I32 x) { ... }
// or
unused I32 y = someValue
```

### Other Syntax Notes

- Semicolons are **optional** — the transpiler inserts them automatically
- `++` / `--` (post-increment/decrement) work as statements
- Full C/C++ interop — `#include` of C headers is allowed
- Cross-platform conditionals: `#if defined(_WIN32)` / `#if defined(__linux__)`
- Templates supported: `template<typename T>` (template-within-template method calls not supported)

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

## Compiler Modes

**Transpile only** (output `.cc` files for manual compilation):
```
unholyc <input_dir> <output_dir> [-I<include_dir> ...]
```

**Compiler driver** (transpile + compile to binary in one step):
```
unholyc <input_dir> -o <output_binary> [-I<include_dir> ...] [-L<libdir> ...] [flags...]
```
Passes remaining flags directly to the C++ compiler (`$CXX`, defaults to `c++`). Generated `.cc` files are cleaned up automatically unless `--preserve-source` is passed.

Both `-Ipath` and `-I path` (space) forms are accepted for `-I` and `-L`.

```
unholyc --version
```

## Coding Patterns

- Namespace-oriented design — not OOP classes
- Keep structs flat; behavior lives in the namespace, not the struct
- Avoid creating new files unless necessary; prefer extending existing `.uhc` files
- Headers (`.uhh`) declare the public API; implementations go in `.uhc`
