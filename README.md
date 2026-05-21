# UnholyC

<img align="right" src="./logo.png" width=10%>

UnholyC (UHC) is a custom programming language that transpiles to C++.

https://www.uhclang.org/

## Build & Install

```bash
bash build-all.sh   # compiles transpiler + stdlib into dist/
bash install.sh     # installs to $HOME/.local and adds to PATH
```

Windows:
```bat
build-all.bat
install.bat
```

A custom prefix can be passed as the first argument:
```bash
bash install.sh /usr/local        # system-wide (needs sudo)
install.bat C:\unholyc
```

The install script copies the binary, headers, and libraries to the prefix and adds the `bin/` directory to your shell's PATH automatically.

## Usage

### Compiler driver — transpile and compile in one step

After installing, the stdlib is found automatically — no flags needed:

```bash
unholyc src/ -o myapp
```

Extra flags are forwarded to the C++ compiler (`$CXX`, defaults to `c++`). The intermediate `.cc` files are deleted automatically; pass `--preserve-source` to keep them.

### Transpile to C++ only

```bash
unholyc <input_dir> <output_dir>
```

### Flags

| Flag | Description |
|------|-------------|
| `-o <file>` | Enable compiler driver mode, set output binary |
| `-I<dir>` or `-I <dir>` | Add include directory (passed to both transpiler and compiler) |
| `-L<dir>` or `-L <dir>` | Add library search path (forwarded to compiler) |
| `-v` | Verbose — print each file transpiled and the compiler command |
| `--preserve-source` | Keep generated `.cc` files after compilation |
| `--version` | Print version and exit |

### Stdlib auto-detection

The transpiler searches for the stdlib in this order and uses the first match:

1. `$UHC_HOME` environment variable
2. Sibling of the binary (`<binary>/../`)
3. `$HOME/.local` (Linux/macOS) or `%USERPROFILE%\.local` (Windows)
4. `/usr/local`

## Examples

See the [`examples/`](examples/) directory for runnable examples covering:

- Types, namespaces, `self` structs
- Lambdas (trailing lambda syntax, cross-file, template types, captures)
- `%T` format specifier — auto-calls namespace `toString` methods
- `List<T>`, `Buffer`, `Matrix`, `VectorF`
- Threads, mutexes
- TCP client/server
