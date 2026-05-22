export const NAMESPACE_GUIDE = `
# Namespaces in UnholyC

Namespaces are the primary organization unit. No classes, no inheritance.

## Basic Namespace with \`self\`
\`\`\`uhc
namespace Counter {
    self {
        I32 value = 0
        I32 step  = 1
    }

    Counter create(I32 step) {
        Counter c
        c.step = step
        return c
    }

    U0 increment(Counter c) {
        c.value += c.step
    }

    U0 print(Counter c) {
        Log.info("value = %d (step %d)", c.value, c.step)
    }
}

// Usage
Counter c = Counter.create(5)
Counter.increment(c)
Counter.increment(c)
Counter.print(c)
\`\`\`

## The \`self\` Keyword
- Replaces \`struct It\` — declares the namespace's struct type
- The namespace name IS the type: \`Counter\` not \`Counter.It\`
- Fields can have default values: \`I32 value = 0\`
- Structs are flat — behavior lives in the namespace

## Reference Semantics (IMPORTANT)
Namespace-self-structs are **passed by reference automatically** in function params.
The transpiler injects \`&\` — you never write it manually.

\`\`\`uhc
// This modifies the caller's counter — c is actually Counter& under the hood
U0 increment(Counter c) {
    c.value += c.step   // mutates caller's value
}
\`\`\`

## Header / Implementation Split
\`\`\`uhc
// mymodule.uhh — declarations only
#pragma once
#include <uhcstd.uhh>

namespace Player {
    self {
        I32 health
        String name
    }
    Player create(String name)
    U0 damage(Player p, I32 amount)
    const I8* toString(Player p)  // enables %T
}
\`\`\`

\`\`\`uhc
// mymodule.uhc — implementations
#include "mymodule.uhh"

namespace Player {
    Player create(String name) {
        Player p
        p.health = 100
        p.name = name
        return p
    }

    U0 damage(Player p, I32 amount) {
        p.health -= amount
        if (p.health < 0) p.health = 0
    }

    const I8* toString(Player p) {
        return String.c(String.format("%T(%d hp)", p.name, p.health))
    }
}
\`\`\`

## Namespace Without \`self\`
Pure utility namespaces — no struct type, just functions:
\`\`\`uhc
namespace Utils {
    I32 clampPositive(I32 v) {
        return v < 0 ? 0 : v
    }
    String hexString(U32 n) {
        return String.format("%x", n)
    }
}
\`\`\`

## Nested Calls
\`\`\`uhc
Log.info(String.c(String.format("player: %T", player)))
\`\`\`
`;

export const LAMBDA_GUIDE = `
# Lambdas in UnholyC

Kotlin-style trailing lambdas. Only form: function parameter + trailing call syntax.

## Declaring a Lambda Parameter
\`\`\`uhc
U0 forEach(I32* arr, I32 len, lambda block(I32) -> U0) {
    for (I32 i = 0; i < len; i++) { block(arr[i]) }
}

U0 applyTwice(I32 x, lambda block(I32) -> U0) {
    block(x)
    block(x * 2)
}

U0 repeat(I32 times, lambda block(I32) -> U0) {
    for (I32 i = 0; i < times; i++) { block(i) }
}
\`\`\`

## Trailing Lambda Call Syntax
The trailing block replaces the last lambda argument:
\`\`\`uhc
forEach(nums, 5) { (n) ->
    Log.info("%d", n)
}

applyTwice(10) { (v) ->
    Log.msg("value: %d", v)
}

repeat(3) { (i) ->
    Log.msg("iteration %d", i)
}
\`\`\`

## Template + Lambda
The transpiler auto-injects \`typename __Block_<name>\` into the template:
\`\`\`uhc
// Declaration
template<typename T>
U0 forEach(List<T> list, lambda block(T) -> U0)

// Call
forEach(myList) { (item) ->
    Log.info("%T", item)
}
\`\`\`

## Capture Behavior
- **No captured locals** → hoisted to named generic lambda (no \`[&]\`)
- **Captures locals** → emitted as inline C++ lambda with \`[&]\` capture list

\`\`\`uhc
I32 limit = 100
forEach(nums, 5) { (n) ->
    if (n > limit) {     // captures limit → [&] lambda
        Log.warn("over limit: %d", n)
    }
}
\`\`\`

## Limitations
- Stored lambdas NOT supported: \`lambda x = ...\` will not compile
- Non-void return type NOT supported
- Works top-level, inside namespaces, and cross-file

## Cross-File: Declare in Header
The transpiler pre-scans \`.uhh\` and compiled \`.hh\` headers.
Always declare lambda-accepting functions in a header before calling them from another file:
\`\`\`uhc
// utils.uhh
namespace Utils {
    U0 forEach(I32* arr, I32 len, lambda block(I32) -> U0)
}
// utils.uhc — implementation
// main.uhc — #include "utils.uhh" first, then call with trailing lambda
\`\`\`
`;

export const SYNTAX_REFERENCE = `
# UnholyC Syntax Quick Reference

## Optional Semicolons
Semicolons are optional — transpiler inserts them:
\`\`\`uhc
I32 x = 5        // fine
I32 y = 10;      // also fine
\`\`\`

## %T Format Specifier
\`%T\` in any format string → rewritten to \`%s\` + \`uhc_tostring(...)\`.
Requires the type to have a \`toString\` method. Falls back to \`"[object]"\`.

\`\`\`uhc
namespace Color {
    self { U8 r; U8 g; U8 b }
    const I8* toString(Color c) {
        return String.c(String.format("rgb(%d,%d,%d)", c.r, c.g, c.b))
    }
}
Color c = {255, 128, 0}
Log.info("color: %T", c)   // → [INFO] color: rgb(255,128,0)

// String has built-in toString — works directly:
String name = "Alice"
Log.info("hello %T", name) // → [INFO] hello Alice
\`\`\`

## \`unused\` Keyword
Silences unused variable/parameter warnings:
\`\`\`uhc
U0 foo(unused I32 x, I32 y) { ... }    // in parameter
unused I32 dbg = computeSomething()     // in body
\`\`\`

## Post-increment / Post-decrement
\`\`\`uhc
i++
i--
\`\`\`

## Cross-Platform Guards
\`\`\`uhc
#if defined(_WIN32) || defined(_WIN64)
    // Windows
#elif defined(__linux__)
    // Linux
#elif defined(__APPLE__)
    // macOS
#else
    // other
#endif
\`\`\`

## Templates
\`\`\`uhc
template<typename T>
T max(T a, T b) {
    return a > b ? a : b
}
\`\`\`
Limitation: template-within-template method calls not supported.

## C/C++ Interop
Full interop — include C headers freely:
\`\`\`uhc
#include <stdio.h>
#include <string.h>
#include <math.h>
\`\`\`

## Array Syntax
\`\`\`uhc
I32 nums[5] = {1, 2, 3, 4, 5}
I8 buf[1024]
I32* ptr = nums
\`\`\`

## Aggregate Initialization
\`\`\`uhc
VectorF v = {1.0f, 2.0f, 3.0f, 0.0f}
Counter c                                 // zero-init with defaults
Counter c = Counter.create(5)            // constructor pattern
\`\`\`

## Function Pointers
\`\`\`uhc
typedef U0* (*MyFn)(U0*)
U0 listen(TcpServer server, U0 (*callback)(TcpClient))
\`\`\`

## File Extensions (IMPORTANT)
- \`.uhc\` — source (never .cpp)
- \`.uhh\` — header (never .hpp)
- \`.cc\` / \`.hh\` — transpiler output (never edit these)
`;
