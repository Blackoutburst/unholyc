export const STDLIB_REFERENCE = `
# UnholyC Standard Library Reference

## Includes
\`\`\`uhc
#include <uhcio.uhh>    // Log, File
#include <uhcstd.uhh>   // String, List, Mutex, Thread, Buffer, Time, DynamicLibrary
#include <uhcmath.uhh>  // Math, VectorF, VectorI, Matrix, UHC_PI
#include <uhcnet.uhh>   // TcpClient, TcpServer
\`\`\`

---

## Log (uhcio.uhh)
Leveled logger with colored console output.

\`\`\`uhc
Log.trace(const I8* fmt, ...)   // TRACE level
Log.debug(const I8* fmt, ...)   // DEBUG level
Log.info (const I8* fmt, ...)   // INFO level
Log.warn (const I8* fmt, ...)   // WARN level
Log.error(const I8* fmt, ...)   // ERROR level
Log.msg  (const I8* fmt, ...)   // no level prefix, no color
\`\`\`

### Log.Level enum values
\`UHC_LOG_TRACE\`, \`UHC_LOG_DEBUG\`, \`UHC_LOG_INFO\`, \`UHC_LOG_WARN\`, \`UHC_LOG_ERROR\`

Supports all printf specifiers + \`%T\` (auto-calls toString).

---

## File (uhcio.uhh)
Simple file read utilities.

\`\`\`uhc
File f = File.open(String filePath)   // fields: handle (FILE*), size (I64)
if (f.size > 0) {
    String content = File.read(f)
}
File.close(f)
\`\`\`

---

## String (uhcstd.uhh)
UTF-8 string with value semantics. Use for all text data.

### Construction
\`\`\`uhc
String s = "hello"                       // implicit from literal
String s = String.from("hello")
String n = String.fromI32(42)
String n = String.fromI64(42LL)
String n = String.fromF32(3.14f)
String n = String.fromF64(3.14)
String f = String.format("%d items", n)  // printf-style, max 4096 bytes
\`\`\`

### Query
\`\`\`uhc
U64       String.length(s)
U8        String.isEmpty(s)
I8        String.charAt(s, I32 i)
U8        String.equals(a, b)          // or: a == b
U8        a != b
U8        String.contains(s, sub)
U8        String.startsWith(s, prefix)
U8        String.endsWith(s, suffix)
I64       String.indexOf(s, sub)       // -1 if not found
I64       String.lastIndexOf(s, sub)
const I8* String.c(s)                  // C string pointer (valid while String alive)
\`\`\`

### Transform
\`\`\`uhc
String String.toUpperCase(s)
String String.toLowerCase(s)
String String.trim(s)
String String.trimStart(s)
String String.trimEnd(s)
String String.replace(s, from, to)
String String.substring(s, start)
String String.substring(s, start, end)  // end exclusive
String String.repeat(s, I32 n)
String String.reverse(s)
String s + other                         // operator+
\`\`\`

### Parse
\`\`\`uhc
I32 String.toI32(s)   // throws on failure
I64 String.toI64(s)
F32 String.toF32(s)
F64 String.toF64(s)
\`\`\`

### Split / Join
\`\`\`uhc
List<String> String.split(s, delim)
String       String.join(List<String> parts, sep)
\`\`\`

---

## List<T> (uhcstd.uhh)
Generic doubly-linked list with operator[] support.

\`\`\`uhc
List<I32> nums
List.add(nums, value)
List.addAt(nums, value, I32 index)
U8  List.remove(nums, value)          // 1 if found/removed
     List.removeAt(nums, I32 index)
T&  List.get(nums, I32 index)
     List.set(nums, I32 index, value)
T&  List.first(nums)
T&  List.last(nums)
U64 List.size(nums)
     List.clear(nums)
nums[i]                                // operator[]
\`\`\`

---

## Mutex (uhcstd.uhh)
Cross-platform mutual exclusion (pthreads on Linux/macOS, CRITICAL_SECTION on Windows).

\`\`\`uhc
Mutex m = Mutex.create()
Mutex.lock(m)
Mutex.unlock(m)
Mutex.destroy(m)
\`\`\`

---

## Thread (uhcstd.uhh)
Lightweight thread creation and joining.

\`\`\`uhc
// Entry point signature
typedef U0* (*Thread.Fn)(U0*)

// Usage
Thread t = Thread.start(myFn, arg)
Thread.join(t)
\`\`\`

---

## Buffer (uhcstd.uhh)
Cursor-based binary buffer with endian control.

\`\`\`uhc
Buffer.setOrder(buf, UHC_LITTLE_ENDIAN)   // or UHC_BIG_ENDIAN
Buffer.reset(buf)                          // reset cursor to 0
// Read (advance cursor)
I8  Buffer.getI8(buf)
U8  Buffer.getU8(buf)
I16 Buffer.getI16(buf)
U16 Buffer.getU16(buf)
I32 Buffer.getI32(buf)
U32 Buffer.getU32(buf)
I64 Buffer.getI64(buf)
U64 Buffer.getU64(buf)
F32 Buffer.getF32(buf)
F64 Buffer.getF64(buf)
// Write (advance cursor)
Buffer.putI8(buf, val)
Buffer.putU8(buf, val)
Buffer.putI16(buf, val)
Buffer.putU16(buf, val)
Buffer.putI32(buf, val)
Buffer.putU32(buf, val)
Buffer.putI64(buf, val)
Buffer.putU64(buf, val)
Buffer.putF32(buf, val)
Buffer.putF64(buf, val)
\`\`\`

Fields: \`handle\` (U0*), \`order\` (U8), \`cursor\` (U32)

---

## Time (uhcstd.uhh)
\`\`\`uhc
Time.sleep(U64 ms)            // sleep milliseconds
U64 t0 = Time.millis()        // monotonic clock in ms — subtract two calls for elapsed
\`\`\`

---

## DynamicLibrary (uhcstd.uhh)
Cross-platform shared library loader.

\`\`\`uhc
DynamicLibrary dll = DynamicLibrary.load("libfoo")  // or full path
if (dll.isValid) {
    U0* fn = DynamicLibrary.get(dll, "symbol_name")
    // cast fn before calling
}
DynamicLibrary.unload(dll)
\`\`\`

Fields: \`handle\`, \`name\` (String), \`isValid\` (U8)

---

## Math (uhcmath.uhh)
\`UHC_PI\` = 3.141592653589793

\`\`\`uhc
F32 Math.rad(F32 degrees)
I8  Math.sign(I32 v)          // -1, 0, 1
I8  Math.fsign(F32 v)
F32 Math.fsignf(F32 v)
I32 Math.clamp(I32 v, I32 min, I32 max)
I32 Math.fclamp(F32 v, F32 min, F32 max)
F32 Math.fclampf(F32 v, F32 min, F32 max)
\`\`\`

---

## VectorF (uhcmath.uhh)
Four-component float vector (x, y, z, w). Supports operators +, -, *, /, ==.

\`\`\`uhc
VectorF v = {1.0f, 2.0f, 3.0f, 0.0f}
VectorF.set(v, F32 x, F32 y, F32 z, F32 w)
VectorF.zero(v)
VectorF.normalize(v)
F32 VectorF.length(v)
F32 VectorF.dot(a, b)
VectorF.cross(result, a, b)
VectorF.add(result, a, b)     // also a + b
VectorF.sub(result, a, b)     // also a - b
VectorF.mul(result, a, b)     // component-wise, also a * b
VectorF.scale(result, a, F32 s)
VectorF.div(result, a, F32 s)
U8 VectorF.equals(a, b)        // also a == b
\`\`\`

---

## VectorI (uhcmath.uhh)
Four-component integer vector (I32 x, y, z, w). Same API as VectorF with I32.
\`VectorI.length\` returns F32.

---

## Matrix (uhcmath.uhh)
4x4 row-major float matrix. Supports operators +, -, *, ==.
Index constants: M00–M33 where M\`row\`\`col\` (e.g. M01 = row 0, col 1).

\`\`\`uhc
Matrix mat                          // initialized to identity
Matrix.setIdentity(mat)
Matrix.multiply(result, left, right) // also left * right
Matrix.copy(src, dest)
Matrix.add(result, a, b)
Matrix.sub(result, a, b)
Matrix.mul(result, a, b)
U8 Matrix.inverse(result, src)       // returns 0 if singular
// Transforms
Matrix.ortho2D(mat, l, r, b, t, near, far)
Matrix.ortho2DVk(mat, l, r, b, t, near, far)  // Vulkan clip space
Matrix.projection(mat, w, h, F32 fov, near, far)
Matrix.scale(mat, F32 x, y, z, w)
Matrix.translate(mat, F32 x, y, z, w)
Matrix.rotate(mat, F32 angle, F32 x, y, z)
Matrix.lookAt(mat, VectorF eye, center, up)
\`\`\`

---

## TcpClient (uhcnet.uhh)
\`\`\`uhc
TcpClient c = TcpClient.create(String ip, I16 port)
TcpClient.destroy(c)
U8 TcpClient.write(c, const I8* buffer, U32 size)
U8 TcpClient.read(c, I8* buffer, U32 size)      // non-blocking / partial
U8 TcpClient.readAll(c, I8* buffer, U32 size)   // blocks until all bytes received
\`\`\`

---

## TcpServer (uhcnet.uhh)
\`\`\`uhc
TcpServer s = TcpServer.create(I16 port, U16 slots)
TcpServer.destroy(s)
TcpServer.listen(s, U0 (*callback)(TcpClient))  // blocking, fires callback per client
U8 TcpServer.write(client, const I8* buf, U32 size)
U8 TcpServer.read(client, I8* buf, U32 size)
U8 TcpServer.readAll(client, I8* buf, U32 size)
TcpServer.broadcast(List<TcpClient> clients, const I8* buf, U32 size)
\`\`\`
`;
