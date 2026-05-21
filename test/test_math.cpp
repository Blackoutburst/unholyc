#include <catch2/catch_all.hpp>
#include <uhcmath.hh>

using Catch::Approx;

// ──────────────────────────────────────────────────────────────────────────────
// Math scalar
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("Math::rad", "[math]") {
    CHECK(Math::rad(0.0f)   == Approx(0.0f));
    CHECK(Math::rad(90.0f)  == Approx((float)M_PI / 2.0f).epsilon(0.001));
    CHECK(Math::rad(180.0f) == Approx((float)M_PI).epsilon(0.001));
    CHECK(Math::rad(360.0f) == Approx(2.0f * (float)M_PI).epsilon(0.001));
}

TEST_CASE("Math::sign", "[math]") {
    CHECK((int)Math::sign(-42) == -1);
    CHECK((int)Math::sign(0)   ==  0);
    CHECK((int)Math::sign(7)   ==  1);
}

TEST_CASE("Math::fsign / fsignf", "[math]") {
    CHECK((int)Math::fsign(-1.5f)  == -1);
    CHECK((int)Math::fsign(0.0f)   ==  0);
    CHECK((int)Math::fsign(3.7f)   ==  1);

    CHECK(Math::fsignf(-1.5f) == Approx(-1.0f));
    CHECK(Math::fsignf(0.0f)  == Approx(0.0f));
    CHECK(Math::fsignf(3.7f)  == Approx(1.0f));
}

TEST_CASE("Math::clamp", "[math]") {
    CHECK(Math::clamp(-5,  0, 100) ==   0);
    CHECK(Math::clamp(50,  0, 100) ==  50);
    CHECK(Math::clamp(150, 0, 100) == 100);
    CHECK(Math::clamp(0,   0, 100) ==   0);
    CHECK(Math::clamp(100, 0, 100) == 100);
}

TEST_CASE("Math::fclamp / fclampf", "[math]") {
    CHECK(Math::fclamp(-0.5f, 0.0f, 1.0f) == 0);
    CHECK(Math::fclamp(1.8f,  0.0f, 1.0f) == 1);

    CHECK(Math::fclampf(-0.5f, 0.0f, 1.0f) == Approx(0.0f));
    CHECK(Math::fclampf(0.5f,  0.0f, 1.0f) == Approx(0.5f));
    CHECK(Math::fclampf(1.8f,  0.0f, 1.0f) == Approx(1.0f));
}

// ──────────────────────────────────────────────────────────────────────────────
// VectorF
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("VectorF default init is zero", "[vectorf]") {
    VectorF::It v;
    CHECK(v.x == 0.0f);
    CHECK(v.y == 0.0f);
    CHECK(v.z == 0.0f);
    CHECK(v.w == 0.0f);
}

TEST_CASE("VectorF::set / zero", "[vectorf]") {
    VectorF::It v;
    VectorF::set(v, 1.0f, 2.0f, 3.0f, 4.0f);
    CHECK(v.x == 1.0f);
    CHECK(v.y == 2.0f);
    CHECK(v.z == 3.0f);
    CHECK(v.w == 4.0f);

    VectorF::zero(v);
    CHECK(v.x == 0.0f);
    CHECK(v.y == 0.0f);
    CHECK(v.z == 0.0f);
    CHECK(v.w == 0.0f);
}

TEST_CASE("VectorF::equals", "[vectorf]") {
    VectorF::It a, b, c;
    VectorF::set(a, 1.0f, 2.0f, 3.0f, 4.0f);
    VectorF::set(b, 1.0f, 2.0f, 3.0f, 4.0f);
    VectorF::set(c, 1.0f, 2.0f, 3.0f, 5.0f);
    CHECK(VectorF::equals(a, b));
    CHECK_FALSE(VectorF::equals(a, c));
}

TEST_CASE("VectorF::length", "[vectorf]") {
    VectorF::It v;
    VectorF::set(v, 3.0f, 4.0f, 0.0f, 0.0f);
    CHECK(VectorF::length(v) == Approx(5.0f));

    VectorF::set(v, 1.0f, 0.0f, 0.0f, 0.0f);
    CHECK(VectorF::length(v) == Approx(1.0f));
}

TEST_CASE("VectorF::normalize", "[vectorf]") {
    VectorF::It v;
    VectorF::set(v, 3.0f, 4.0f, 0.0f, 0.0f);
    VectorF::normalize(v);
    CHECK(VectorF::length(v) == Approx(1.0f).epsilon(0.001f));
    CHECK(v.x == Approx(0.6f).epsilon(0.001f));
    CHECK(v.y == Approx(0.8f).epsilon(0.001f));

    // zero vector — should not crash
    VectorF::It z;
    VectorF::normalize(z);
    CHECK(z.x == 0.0f);
}

TEST_CASE("VectorF::dot", "[vectorf]") {
    VectorF::It right, up, fwd;
    VectorF::set(right, 1.0f, 0.0f, 0.0f, 0.0f);
    VectorF::set(up,    0.0f, 1.0f, 0.0f, 0.0f);
    VectorF::set(fwd,   0.0f, 0.0f, 1.0f, 0.0f);

    CHECK(VectorF::dot(right, up)    == Approx(0.0f));
    CHECK(VectorF::dot(right, right) == Approx(1.0f));
    CHECK(VectorF::dot(right, fwd)   == Approx(0.0f));
}

TEST_CASE("VectorF::cross", "[vectorf]") {
    VectorF::It right, up, result;
    VectorF::set(right, 1.0f, 0.0f, 0.0f, 0.0f);
    VectorF::set(up,    0.0f, 1.0f, 0.0f, 0.0f);

    VectorF::cross(result, right, up);
    CHECK(result.x == Approx(0.0f));
    CHECK(result.y == Approx(0.0f));
    CHECK(result.z == Approx(1.0f));
    CHECK(result.w == 0.0f);
}

TEST_CASE("VectorF arithmetic functions", "[vectorf]") {
    VectorF::It p, q, r;
    VectorF::set(p, 1.0f, 2.0f, 3.0f, 0.0f);
    VectorF::set(q, 4.0f, 5.0f, 6.0f, 0.0f);

    VectorF::add(r, p, q);
    CHECK(r.x == Approx(5.0f));
    CHECK(r.y == Approx(7.0f));
    CHECK(r.z == Approx(9.0f));

    VectorF::sub(r, q, p);
    CHECK(r.x == Approx(3.0f));
    CHECK(r.y == Approx(3.0f));
    CHECK(r.z == Approx(3.0f));

    VectorF::mul(r, p, q);
    CHECK(r.x == Approx(4.0f));
    CHECK(r.y == Approx(10.0f));
    CHECK(r.z == Approx(18.0f));

    VectorF::scale(r, p, 2.0f);
    CHECK(r.x == Approx(2.0f));
    CHECK(r.y == Approx(4.0f));
    CHECK(r.z == Approx(6.0f));

    VectorF::div(r, p, 2.0f);
    CHECK(r.x == Approx(0.5f));
    CHECK(r.y == Approx(1.0f));
    CHECK(r.z == Approx(1.5f));
}

TEST_CASE("VectorF operators", "[vectorf]") {
    VectorF::It p = {1.0f, 2.0f, 3.0f, 0.0f};
    VectorF::It q = {4.0f, 5.0f, 6.0f, 0.0f};

    auto sum = p + q;
    CHECK(sum.x == Approx(5.0f));
    CHECK(sum.y == Approx(7.0f));

    auto diff = q - p;
    CHECK(diff.x == Approx(3.0f));

    auto neg = -p;
    CHECK(neg.x == Approx(-1.0f));
    CHECK(neg.y == Approx(-2.0f));

    auto scaled = p * 3.0f;
    CHECK(scaled.x == Approx(3.0f));
    CHECK(scaled.y == Approx(6.0f));

    auto divided = q / 2.0f;
    CHECK(divided.x == Approx(2.0f));

    auto comp = p * q;
    CHECK(comp.x == Approx(4.0f));

    VectorF::It a = {1.0f, 2.0f, 3.0f, 4.0f};
    VectorF::It b = {1.0f, 2.0f, 3.0f, 4.0f};
    VectorF::It c = {1.0f, 2.0f, 3.0f, 5.0f};
    CHECK(a == b);
    CHECK_FALSE(a == c);
}

// ──────────────────────────────────────────────────────────────────────────────
// VectorI
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("VectorI default init is zero", "[vectori]") {
    VectorI::It v;
    CHECK(v.x == 0);
    CHECK(v.y == 0);
    CHECK(v.z == 0);
    CHECK(v.w == 0);
}

TEST_CASE("VectorI::set / zero / equals", "[vectori]") {
    VectorI::It a, b;
    VectorI::set(a, 1, 2, 3, 4);
    CHECK(a.x == 1);
    CHECK(a.y == 2);
    CHECK(a.z == 3);
    CHECK(a.w == 4);

    VectorI::set(b, 1, 2, 3, 4);
    CHECK(VectorI::equals(a, b));

    VectorI::zero(a);
    CHECK(a.x == 0);
    CHECK(a.y == 0);
    CHECK(a.z == 0);
    CHECK(a.w == 0);
    CHECK_FALSE(VectorI::equals(a, b));
}

TEST_CASE("VectorI::length", "[vectori]") {
    VectorI::It v;
    VectorI::set(v, 3, 4, 0, 0);
    CHECK(VectorI::length(v) == Approx(5.0f));
}

TEST_CASE("VectorI::dot", "[vectori]") {
    VectorI::It a, b;
    VectorI::set(a, 1, 0, 0, 0);
    VectorI::set(b, 0, 1, 0, 0);
    CHECK(VectorI::dot(a, b) == 0);

    VectorI::set(b, 2, 0, 0, 0);
    CHECK(VectorI::dot(a, b) == 2);
}

TEST_CASE("VectorI::cross", "[vectori]") {
    VectorI::It right, up, result;
    VectorI::set(right, 1, 0, 0, 0);
    VectorI::set(up,    0, 1, 0, 0);
    VectorI::cross(result, right, up);
    CHECK(result.x == 0);
    CHECK(result.y == 0);
    CHECK(result.z == 1);
    CHECK(result.w == 0);
}

TEST_CASE("VectorI arithmetic functions", "[vectori]") {
    VectorI::It p, q, r;
    VectorI::set(p, 1, 2, 3, 0);
    VectorI::set(q, 4, 5, 6, 0);

    VectorI::add(r, p, q);
    CHECK(r.x == 5); CHECK(r.y == 7); CHECK(r.z == 9);

    VectorI::sub(r, q, p);
    CHECK(r.x == 3); CHECK(r.y == 3); CHECK(r.z == 3);

    VectorI::mul(r, p, q);
    CHECK(r.x == 4); CHECK(r.y == 10); CHECK(r.z == 18);

    VectorI::scale(r, p, 2);
    CHECK(r.x == 2); CHECK(r.y == 4); CHECK(r.z == 6);

    VectorI::div(r, p, 2);
    CHECK(r.x == 0); CHECK(r.y == 1); CHECK(r.z == 1);
}

TEST_CASE("VectorI operators", "[vectori]") {
    VectorI::It p = {1, 2, 3, 0};
    VectorI::It q = {4, 5, 6, 0};

    auto sum = p + q;
    CHECK(sum.x == 5);

    auto neg = -p;
    CHECK(neg.x == -1);

    auto scaled = p * 3;
    CHECK(scaled.x == 3);

    auto divided = q / 2;
    CHECK(divided.x == 2);

    VectorI::It a = {1, 2, 3, 4};
    VectorI::It b = {1, 2, 3, 4};
    CHECK(a == b);
}
