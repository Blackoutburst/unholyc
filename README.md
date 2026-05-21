# UnholyC

<img align="right" src="./logo.png" width=10%>

UnholyC (UHC) is a custom programming language that transpiles to C++.

https://www.uhclang.org/

## Build

```bash
bash build-all.sh
```

Compiles the transpiler, then transpiles and builds the standard library and graphics library into `dist/`.

## Usage

### Transpile to C++

```bash
unholyc <input_dir> <output_dir> [-I<include_dir> ...]
```

```bash
unholyc src/ out/ -Idist/include
```

### Compiler driver — transpile and compile in one step

```bash
unholyc <input_dir> -o <output_binary> [-I<include_dir> ...] [flags...]
```

```bash
unholyc src/ -o myapp -Idist/include -Ldist/lib -luhc
```

Extra flags are forwarded to the C++ compiler (`$CXX`, defaults to `c++`). The intermediate `.cc` files are deleted automatically; pass `--preserve-source` to keep them.

### Flags

| Flag | Description |
|------|-------------|
| `-o <file>` | Enable compiler driver mode, set output binary |
| `-I<dir>` or `-I <dir>` | Add include directory (passed to both transpiler and compiler) |
| `-L<dir>` or `-L <dir>` | Add library search path (forwarded to compiler) |
| `-v` | Verbose — print each file transpiled and the compiler command |
| `--preserve-source` | Keep generated `.cc` files after compilation |
| `--version` | Print version and exit |

## Examples

See the [`examples/`](examples/) directory for runnable examples covering:

- Types, namespaces, `self` structs
- Lambdas (trailing lambda syntax, cross-file, template types, captures)
- `%T` format specifier — auto-calls namespace `toString` methods
- `List<T>`, `Buffer`, `Matrix`, `VectorF`
- Threads, mutexes
- TCP client/server
