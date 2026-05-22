export const BUILD_GUIDE = `
# Building UnholyC Projects

## After Editing Transpiler / uhcstd / Graphics

**Windows:**
\`\`\`
build-all.bat
install.bat
\`\`\`

**Linux / macOS:**
\`\`\`bash
bash build-all.sh
bash install.sh
\`\`\`

Pipeline: compile transpiler → transpile uhcstd + graphics → build .a libs.
4k+ lines of UHC go through the transpiler — errors surface immediately.

---

## Compiler: Driver Mode (transpile + compile to binary)
\`\`\`
unholyc <input_dir> -o <output_binary> [flags...]
\`\`\`
- Stdlib auto-detected after install — no -I or -L needed
- Remaining flags forwarded to $CXX (defaults to c++)
- Generated .cc files removed unless \`--preserve-source\`

\`\`\`bash
unholyc myproject/ -o myapp
unholyc myproject/ -o myapp -O2
unholyc myproject/ -o myapp --preserve-source
\`\`\`

## Compiler: Transpile-Only Mode (output .cc files)
\`\`\`
unholyc <input_dir> <output_dir> [-I<include_dir> ...]
\`\`\`

## Version
\`\`\`
unholyc --version
\`\`\`

---

## Stdlib Auto-Detection Order
1. \`$UHC_HOME\` env var (Linux/macOS) / \`%UHC_HOME%\` (Windows)
2. Sibling of binary: \`<binary>/../\`
3. \`$HOME/.local\` (Linux/macOS) / \`%USERPROFILE%\\.local\` (Windows)
4. \`/usr/local\` (Linux/macOS)

Explicit \`-I\` / \`-L\` / \`-luhc\` flags override auto-detection.
Both \`-Ipath\` and \`-I path\` (space form) accepted.

---

## Project Directory Layout
\`\`\`
myproject/
  main.uhc           entry point — must have I32 main()
  mymodule.uhh       header — declare namespace API
  mymodule.uhc       implementation — define namespace methods
\`\`\`

## Minimal main.uhc
\`\`\`uhc
#include <uhcio.uhh>

I32 main() {
    Log.info("hello world")
    return 0
}
\`\`\`
`;

export const CODING_PATTERNS = `
# UnholyC Coding Patterns & Idioms

## Constructor Pattern
\`\`\`uhc
namespace Socket {
    self {
        I32 fd = -1
        U8  connected = 0
    }

    Socket create(String ip, I16 port) {
        Socket s
        // ... setup
        s.connected = 1
        return s
    }

    U0 destroy(Socket s) {
        // ... cleanup
        s.connected = 0
    }
}
\`\`\`

## toString for %T
\`\`\`uhc
namespace Vec2 {
    self { F32 x; F32 y }

    const I8* toString(Vec2 v) {
        return String.c(String.format("(%.2f, %.2f)", v.x, v.y))
    }
}

Vec2 pos = {3.0f, 4.0f}
Log.info("position: %T", pos)  // → [INFO] position: (3.00, 4.00)
\`\`\`

## Threaded Server Pattern
\`\`\`uhc
namespace Server {
    Mutex mu
    List<TcpClient> clients

    U0* clientWorker(U0* arg) {
        TcpClient client = *(TcpClient*)arg
        I8 buf[4096]
        while (TcpClient.readAll(client, buf, 4)) {
            // process buf...
        }
        Mutex.lock(Server.mu)
        List.remove(Server.clients, client)
        Mutex.unlock(Server.mu)
        return NULL
    }
}

U0 onConnect(TcpClient client) {
    Mutex.lock(Server.mu)
    List.add(Server.clients, client)
    Mutex.unlock(Server.mu)
    Thread t = Thread.start(Server.clientWorker, &client)
    Thread.join(t)
}

I32 main() {
    Server.mu = Mutex.create()
    TcpServer srv = TcpServer.create(8080, 64)
    TcpServer.listen(srv, onConnect)
    return 0
}
\`\`\`

## File Processing Pattern
\`\`\`uhc
File f = File.open(path)
if (f.size <= 0) {
    Log.error("cannot open: %T", path)
    return 1
}
String content = File.read(f)
File.close(f)

List<String> lines = String.split(content, "\\n")
U64 n = List.size(lines)
for (U64 i = 0; i < n; i++) {
    String line = List.get(lines, (I32)i)
    // process line...
}
\`\`\`

## Buffer / Network Protocol Pattern
\`\`\`uhc
// Write a message: [U32 length][payload]
U0 sendMessage(TcpClient client, String msg) {
    const I8* data = String.c(msg)
    U32 len = (U32)String.length(msg)
    // send length prefix then payload
    TcpClient.write(client, (const I8*)&len, 4)
    TcpClient.write(client, data, len)
}
\`\`\`

## Lambda for Collection Processing
\`\`\`uhc
namespace Scores {
    U0 printAbove(List<I32> scores, I32 threshold, lambda block(I32) -> U0)
}

// Call site
Scores.printAbove(scores, 50) { (s) ->
    Log.info("high score: %d", s)
}
\`\`\`

---

## Common Mistakes to Avoid

| Mistake | Fix |
|---------|-----|
| Using .cpp/.hpp extensions | Always use .uhc/.uhh |
| Writing \`&\` for self params | Transpiler injects it automatically |
| \`lambda x = ...\` stored lambda | Not supported — inline only |
| Non-void lambda return | Not supported |
| Editing .cc/.hh output files | Don't — regenerated on every build |
| Using \`bool\` type | Use \`U8\` (0/non-zero) |
| \`I8*\` for text data | Use \`String\` |
| Template-within-template calls | Not supported |
| Forgetting \`#pragma once\` in .uhh | Always add it |
`;

export const OVERVIEW = `
# UnholyC (UHC) — Language Overview

UnholyC is a HolyC-inspired language that transpiles to C++17.
Single-file transpiler. Namespace-oriented design. Full C/C++ interop.

## Core Concepts

### File Extensions
- \`.uhc\` — source (like .cpp)
- \`.uhh\` — header (like .hpp)
- \`.cc\` / \`.hh\` — transpiler output (never edit)

### Language Pillars
1. **Namespaces** — primary organization unit; \`self\` declares struct type
2. **Reference semantics** — namespace-self params auto-passed by reference
3. **Lambdas** — Kotlin-style trailing lambdas, capture-aware
4. **%T specifier** — auto-format any type with a \`toString\` method
5. **HolyC types** — U0/U8/U16/U32/U64, I8/I16/I32/I64, F32/F64
6. **Optional semicolons** — transpiler inserts them

### Stdlib Modules
| Header | Contents |
|--------|----------|
| uhcstd.uhh | String, List<T>, Mutex, Thread, Buffer, Time, DynamicLibrary |
| uhcio.uhh  | Log (leveled logger), File |
| uhcmath.uhh | Math, VectorF, VectorI, Matrix, UHC_PI |
| uhcnet.uhh  | TcpClient, TcpServer |

### Compile
\`\`\`
unholyc myproject/ -o myapp      # driver mode
unholyc myproject/ out/ -Ipath   # transpile-only
\`\`\`

### Minimal Program
\`\`\`uhc
#include <uhcio.uhh>

I32 main() {
    Log.info("hello from UnholyC")
    return 0
}
\`\`\`

### Namespace + Self Example
\`\`\`uhc
namespace Counter {
    self { I32 value = 0; I32 step = 1 }
    Counter create(I32 step) {
        Counter c
        c.step = step
        return c
    }
    U0 increment(Counter c) { c.value += c.step }
}

I32 main() {
    Counter c = Counter.create(5)
    Counter.increment(c)     // c.value == 5
    Counter.increment(c)     // c.value == 10
    Log.info("value = %d", c.value)
    return 0
}
\`\`\`
`;
