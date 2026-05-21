#include <catch2/catch_all.hpp>
#include <uhcmath.hh>

using Catch::Approx;

TEST_CASE("Matrix default is identity", "[matrix]") {
    Matrix::It m;
    CHECK(m.values[M00] == 1.0f);
    CHECK(m.values[M11] == 1.0f);
    CHECK(m.values[M22] == 1.0f);
    CHECK(m.values[M33] == 1.0f);
    CHECK(m.values[M01] == 0.0f);
    CHECK(m.values[M10] == 0.0f);
    CHECK(m.values[M30] == 0.0f);
}

TEST_CASE("Matrix::setIdentity", "[matrix]") {
    Matrix::It m;
    Matrix::scale(m, 2.0f, 2.0f, 2.0f, 2.0f);
    Matrix::setIdentity(m);
    CHECK(m.values[M00] == 1.0f);
    CHECK(m.values[M11] == 1.0f);
    CHECK(m.values[M01] == 0.0f);
}

TEST_CASE("Matrix::copy", "[matrix]") {
    Matrix::It src, dst;
    Matrix::scale(src, 3.0f, 5.0f, 1.0f, 1.0f);
    Matrix::copy(src, dst);
    CHECK(dst.values[M00] == src.values[M00]);
    CHECK(dst.values[M11] == src.values[M11]);
    for (int i = 0; i < 16; i++)
        CHECK(dst.values[i] == src.values[i]);
}

TEST_CASE("Matrix::scale on identity", "[matrix]") {
    Matrix::It m;
    Matrix::scale(m, 2.0f, 3.0f, 4.0f, 1.0f);
    CHECK(m.values[M00] == Approx(2.0f));
    CHECK(m.values[M11] == Approx(3.0f));
    CHECK(m.values[M22] == Approx(4.0f));
    CHECK(m.values[M33] == Approx(1.0f));
    CHECK(m.values[M01] == 0.0f);
}

TEST_CASE("Matrix::translate on identity", "[matrix]") {
    Matrix::It m;
    Matrix::translate(m, 5.0f, 10.0f, 0.0f, 0.0f);
    CHECK(m.values[M30] == Approx(5.0f));
    CHECK(m.values[M31] == Approx(10.0f));
    CHECK(m.values[M32] == Approx(0.0f));
    CHECK(m.values[M00] == 1.0f);
    CHECK(m.values[M11] == 1.0f);
}

TEST_CASE("Matrix::multiply identity * A = A", "[matrix]") {
    Matrix::It identity, a, result;
    Matrix::scale(a, 2.0f, 3.0f, 1.0f, 1.0f);
    Matrix::multiply(result, identity, a);
    for (int i = 0; i < 16; i++)
        CHECK(result.values[i] == Approx(a.values[i]));
}

TEST_CASE("Matrix::mul alias for multiply", "[matrix]") {
    Matrix::It a, b, r1, r2;
    Matrix::scale(a, 2.0f, 1.0f, 1.0f, 1.0f);
    Matrix::translate(b, 3.0f, 0.0f, 0.0f, 0.0f);
    Matrix::multiply(r1, a, b);
    Matrix::mul(r2, a, b);
    for (int i = 0; i < 16; i++)
        CHECK(r1.values[i] == r2.values[i]);
}

TEST_CASE("Matrix::add element-wise", "[matrix]") {
    Matrix::It a, b, result;
    Matrix::add(result, a, a);
    CHECK(result.values[M00] == Approx(2.0f));
    CHECK(result.values[M11] == Approx(2.0f));
    CHECK(result.values[M01] == Approx(0.0f));
}

TEST_CASE("Matrix::sub element-wise", "[matrix]") {
    Matrix::It a, result;
    Matrix::sub(result, a, a);
    for (int i = 0; i < 16; i++)
        CHECK(result.values[i] == Approx(0.0f));
}

TEST_CASE("Matrix::rotate zero angle leaves identity", "[matrix]") {
    Matrix::It m;
    Matrix::rotate(m, 0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(m.values[M00] == Approx(1.0f).epsilon(0.001f));
    CHECK(m.values[M11] == Approx(1.0f).epsilon(0.001f));
    CHECK(m.values[M01] == Approx(0.0f).margin(0.001f));
}

TEST_CASE("Matrix::rotate 90 degrees around Z", "[matrix]") {
    Matrix::It m;
    Matrix::rotate(m, (float)M_PI / 2.0f, 0.0f, 0.0f, 1.0f);
    CHECK(m.values[M00] == Approx(0.0f).margin(0.001f));
    CHECK(m.values[M01] == Approx(1.0f).epsilon(0.001f));
    CHECK(m.values[M10] == Approx(-1.0f).epsilon(0.001f));
    CHECK(m.values[M11] == Approx(0.0f).margin(0.001f));
    CHECK(m.values[M22] == Approx(1.0f).epsilon(0.001f));
}

TEST_CASE("Matrix::ortho2D spot-check", "[matrix]") {
    Matrix::It m;
    Matrix::ortho2D(m, 0.0f, 800.0f, 0.0f, 600.0f, -1.0f, 1.0f);
    CHECK(m.values[M00] == Approx(2.0f / 800.0f));
    CHECK(m.values[M11] == Approx(2.0f / 600.0f));
    CHECK(m.values[M22] == Approx(-1.0f));
    CHECK(m.values[M30] == Approx(-1.0f));
    CHECK(m.values[M31] == Approx(-1.0f));
    CHECK(m.values[M33] == Approx(1.0f));
}

TEST_CASE("Matrix::projection spot-check", "[matrix]") {
    Matrix::It m;
    Matrix::projection(m, 800.0f, 600.0f, 60.0f, 0.1f, 1000.0f);
    // M00 and M11 must be positive, M23 must be -1 (perspective divide)
    CHECK(m.values[M00] > 0.0f);
    CHECK(m.values[M11] > 0.0f);
    CHECK(m.values[M23] == Approx(-1.0f));
}

TEST_CASE("Matrix::lookAt basic", "[matrix]") {
    VectorF::It eye, center, up;
    VectorF::set(eye,    0.0f, 0.0f, 5.0f, 1.0f);
    VectorF::set(center, 0.0f, 0.0f, 0.0f, 1.0f);
    VectorF::set(up,     0.0f, 1.0f, 0.0f, 0.0f);
    Matrix::It view;
    Matrix::lookAt(view, eye, center, up);
    // Looking down -Z: M33 = 1, view[M22] = 1 (f is (0,0,-1), so -f.z=1)
    CHECK(view.values[M33] == Approx(1.0f));
}

TEST_CASE("Matrix::inverse identity", "[matrix]") {
    Matrix::It src, result;
    int ok = Matrix::inverse(result, src);
    REQUIRE(ok == 1);
    for (int i = 0; i < 16; i++)
        CHECK(result.values[i] == Approx(src.values[i]));
}

TEST_CASE("Matrix::inverse diagonal matrix", "[matrix]") {
    Matrix::It src, result;
    Matrix::scale(src, 2.0f, 4.0f, 5.0f, 1.0f);
    int ok = Matrix::inverse(result, src);
    REQUIRE(ok == 1);
    CHECK(result.values[M00] == Approx(0.5f).epsilon(0.001f));
    CHECK(result.values[M11] == Approx(0.25f).epsilon(0.001f));
    CHECK(result.values[M22] == Approx(0.2f).epsilon(0.001f));
    CHECK(result.values[M33] == Approx(1.0f).epsilon(0.001f));
}

TEST_CASE("Matrix::inverse singular returns 0", "[matrix]") {
    Matrix::It src, result;
    for (int i = 0; i < 16; i++) src.values[i] = 0.0f;
    int ok = Matrix::inverse(result, src);
    CHECK(ok == 0);
}

TEST_CASE("Matrix operators", "[matrix]") {
    Matrix::It a;
    auto doubled = a + a;
    CHECK(doubled.values[M00] == Approx(2.0f));
    CHECK(doubled.values[M01] == Approx(0.0f));

    auto zeroed = a - a;
    CHECK(zeroed.values[M00] == Approx(0.0f));

    auto scaled = a * 2.0f;
    CHECK(scaled.values[M00] == Approx(2.0f));
    CHECK(scaled.values[M01] == Approx(0.0f));

    CHECK(a == a);
}
