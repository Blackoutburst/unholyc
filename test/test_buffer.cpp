#include <catch2/catch_all.hpp>
#include <uhcstd.hh>
#include <stdlib.h>

using Catch::Approx;

static Buffer::It makeBuffer(unsigned char order) {
    Buffer::It buf;
    buf.handle = malloc(256);
    buf.cursor = 0;
    Buffer::setOrder(buf, order);
    return buf;
}

static void freeBuffer(Buffer::It& buf) {
    free(buf.handle);
}

// ──────────────────────────────────────────────────────────────────────────────
// Big-endian round-trips
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Buffer big-endian U8 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU8(buf, 0xAB);
    Buffer::reset(buf);
    CHECK(Buffer::getU8(buf) == 0xAB);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian I8 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putI8(buf, -42);
    Buffer::reset(buf);
    CHECK((int)Buffer::getI8(buf) == -42);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian I16 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putI16(buf, -1000);
    Buffer::reset(buf);
    CHECK(Buffer::getI16(buf) == -1000);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian U16 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU16(buf, 0xBEEF);
    Buffer::reset(buf);
    CHECK(Buffer::getU16(buf) == 0xBEEF);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian I32 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putI32(buf, -123456789);
    Buffer::reset(buf);
    CHECK(Buffer::getI32(buf) == -123456789);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian U32 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU32(buf, 0xDEADBEEF);
    Buffer::reset(buf);
    CHECK(Buffer::getU32(buf) == 0xDEADBEEFu);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian I64 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putI64(buf, -9000000000000LL);
    Buffer::reset(buf);
    CHECK(Buffer::getI64(buf) == -9000000000000LL);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian U64 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU64(buf, 0xCAFEBABEDEAD0123ULL);
    Buffer::reset(buf);
    CHECK(Buffer::getU64(buf) == 0xCAFEBABEDEAD0123ULL);
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian F32 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putF32(buf, 3.14f);
    Buffer::reset(buf);
    CHECK(Buffer::getF32(buf) == Approx(3.14f).epsilon(0.0001f));
    freeBuffer(buf);
}

TEST_CASE("Buffer big-endian F64 round-trip", "[buffer][be]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putF64(buf, 2.718281828);
    Buffer::reset(buf);
    CHECK(Buffer::getF64(buf) == Approx(2.718281828).epsilon(1e-9));
    freeBuffer(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// Little-endian round-trips
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Buffer little-endian U32 round-trip", "[buffer][le]") {
    auto buf = makeBuffer(UHC_LITTLE_ENDIAN);
    Buffer::putU32(buf, 0xDEADBEEF);
    Buffer::reset(buf);
    CHECK(Buffer::getU32(buf) == 0xDEADBEEFu);
    freeBuffer(buf);
}

TEST_CASE("Buffer little-endian I64 round-trip", "[buffer][le]") {
    auto buf = makeBuffer(UHC_LITTLE_ENDIAN);
    Buffer::putI64(buf, -9000000000000LL);
    Buffer::reset(buf);
    CHECK(Buffer::getI64(buf) == -9000000000000LL);
    freeBuffer(buf);
}

TEST_CASE("Buffer little-endian F32 round-trip", "[buffer][le]") {
    auto buf = makeBuffer(UHC_LITTLE_ENDIAN);
    Buffer::putF32(buf, -1.5f);
    Buffer::reset(buf);
    CHECK(Buffer::getF32(buf) == Approx(-1.5f));
    freeBuffer(buf);
}

// ──────────────────────────────────────────────────────────────────────────────
// Cursor and order management
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Buffer cursor advances on writes", "[buffer]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU8(buf,  0x01);
    CHECK(buf.cursor == 1);
    Buffer::putI16(buf, 100);
    CHECK(buf.cursor == 3);
    Buffer::putU32(buf, 0);
    CHECK(buf.cursor == 7);
    freeBuffer(buf);
}

TEST_CASE("Buffer::reset rewinds cursor", "[buffer]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU32(buf, 42);
    REQUIRE(buf.cursor == 4);
    Buffer::reset(buf);
    CHECK(buf.cursor == 0);
    freeBuffer(buf);
}

TEST_CASE("Buffer sequential multi-type write/read", "[buffer]") {
    auto buf = makeBuffer(UHC_BIG_ENDIAN);
    Buffer::putU8(buf,  0xAB);
    Buffer::putI16(buf, -1000);
    Buffer::putU32(buf, 123456789u);
    Buffer::putF32(buf, 3.14f);

    Buffer::reset(buf);
    CHECK(Buffer::getU8(buf)  == 0xAB);
    CHECK(Buffer::getI16(buf) == -1000);
    CHECK(Buffer::getU32(buf) == 123456789u);
    CHECK(Buffer::getF32(buf) == Approx(3.14f).epsilon(0.0001f));
    freeBuffer(buf);
}
