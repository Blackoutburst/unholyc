#include <catch2/catch_all.hpp>
#include <uhcstd.hh>
#include <uhcmath.hh>

TEST_CASE("List empty state", "[list]") {
    List::It<int> lst;
    CHECK(List::size(lst) == 0);
    CHECK(List::first(lst) == nullptr);
    CHECK(List::last(lst)  == nullptr);
}

TEST_CASE("List::add and size", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 20);
    List::add(lst, 30);
    CHECK(List::size(lst) == 3);
}

TEST_CASE("List::first and last", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 20);
    List::add(lst, 30);
    CHECK(*List::first(lst) == 10);
    CHECK(*List::last(lst)  == 30);
}

TEST_CASE("List::get", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 20);
    List::add(lst, 30);
    CHECK(*List::get(lst, 0) == 10);
    CHECK(*List::get(lst, 1) == 20);
    CHECK(*List::get(lst, 2) == 30);
}

TEST_CASE("List::set", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 20);
    List::set(lst, 1, 99);
    CHECK(*List::get(lst, 1) == 99);
}

TEST_CASE("List::addAt", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 30);
    List::addAt(lst, 20, 1);
    CHECK(List::size(lst) == 3);
    CHECK(*List::get(lst, 0) == 10);
    CHECK(*List::get(lst, 1) == 20);
    CHECK(*List::get(lst, 2) == 30);
}

TEST_CASE("List::addAt at front", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::addAt(lst, 5, 0);
    CHECK(*List::first(lst) == 5);
    CHECK(List::size(lst) == 2);
}

TEST_CASE("List::remove found", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 20);
    List::add(lst, 30);
    unsigned char removed = List::remove(lst, 20);
    CHECK(removed != 0);
    CHECK(List::size(lst) == 2);
    CHECK(*List::get(lst, 0) == 10);
    CHECK(*List::get(lst, 1) == 30);
}

TEST_CASE("List::remove not found", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    unsigned char removed = List::remove(lst, 99);
    CHECK(removed == 0);
    CHECK(List::size(lst) == 1);
}

TEST_CASE("List::removeAt", "[list]") {
    List::It<int> lst;
    List::add(lst, 10);
    List::add(lst, 20);
    List::add(lst, 30);
    List::removeAt(lst, 1);
    CHECK(List::size(lst) == 2);
    CHECK(*List::get(lst, 0) == 10);
    CHECK(*List::get(lst, 1) == 30);
}

TEST_CASE("List::clear", "[list]") {
    List::It<int> lst;
    List::add(lst, 1);
    List::add(lst, 2);
    List::clear(lst);
    CHECK(List::size(lst) == 0);
    CHECK(List::first(lst) == nullptr);
}

TEST_CASE("List works with float elements", "[list]") {
    List::It<float> lst;
    List::add(lst, 1.5f);
    List::add(lst, 2.5f);
    CHECK(List::size(lst) == 2);
    CHECK(*List::first(lst) == 1.5f);
    CHECK(*List::last(lst)  == 2.5f);
}

TEST_CASE("List works with VectorF elements", "[list]") {
    List::It<VectorF::It> lst;
    VectorF::It v1 = {1.0f, 0.0f, 0.0f, 0.0f};
    VectorF::It v2 = {0.0f, 1.0f, 0.0f, 0.0f};
    List::add(lst, v1);
    List::add(lst, v2);
    CHECK(List::size(lst) == 2);
    CHECK(List::get(lst, 0)->x == 1.0f);
    CHECK(List::get(lst, 1)->y == 1.0f);
}
