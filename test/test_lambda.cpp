#include <catch2/catch_all.hpp>
#include <uhcstd.hh>
#include <lambda.hh>

static List::It<int> makeList(std::initializer_list<int> vals) {
    List::It<int> lst;
    for (int v : vals) List::add(lst, v);
    return lst;
}

TEST_CASE("forEach visits every element", "[lambda]") {
    auto nums = makeList({1, 2, 3, 4, 5});
    int count = 0;
    forEach(nums, [&](int) { count++; });
    CHECK(count == 5);
}

TEST_CASE("forEach visits in order", "[lambda]") {
    auto nums = makeList({10, 20, 30});
    std::vector<int> seen;
    forEach(nums, [&](int n) { seen.push_back(n); });
    REQUIRE(seen.size() == 3);
    CHECK(seen[0] == 10);
    CHECK(seen[1] == 20);
    CHECK(seen[2] == 30);
}

TEST_CASE("forEach on empty list does not call block", "[lambda]") {
    List::It<int> empty;
    int count = 0;
    forEach(empty, [&](int) { count++; });
    CHECK(count == 0);
}

TEST_CASE("forEach captures local variable", "[lambda]") {
    auto nums = makeList({1, 2, 3, 4, 5});
    int sum = 0;
    forEach(nums, [&](int n) { sum += n; });
    CHECK(sum == 15);
}

TEST_CASE("filter keeps matching elements", "[lambda]") {
    auto nums = makeList({1, 2, 3, 4, 5});
    auto evens = filter(nums, [](int n) -> unsigned char { return n % 2 == 0; });
    REQUIRE(List::size(evens) == 2);
    CHECK(*List::get(evens, 0) == 2);
    CHECK(*List::get(evens, 1) == 4);
}

TEST_CASE("filter all match", "[lambda]") {
    auto nums = makeList({2, 4, 6});
    auto result = filter(nums, [](int n) -> unsigned char { return n % 2 == 0; });
    CHECK(List::size(result) == 3);
}

TEST_CASE("filter none match returns empty", "[lambda]") {
    auto nums = makeList({1, 3, 5});
    auto result = filter(nums, [](int n) -> unsigned char { return n % 2 == 0; });
    CHECK(List::size(result) == 0);
}

TEST_CASE("filter captures local variable (predicate)", "[lambda]") {
    auto nums = makeList({1, 2, 3, 4, 5, 6});
    int threshold = 3;
    auto big = filter(nums, [&](int n) -> unsigned char { return n > threshold; });
    REQUIRE(List::size(big) == 3);
    CHECK(*List::get(big, 0) == 4);
}

TEST_CASE("map transforms each element", "[lambda]") {
    auto nums = makeList({1, 2, 3, 4, 5});
    auto doubled = map(nums, [](int n) -> int { return n * 2; });
    REQUIRE(List::size(doubled) == 5);
    CHECK(*List::get(doubled, 0) == 2);
    CHECK(*List::get(doubled, 4) == 10);
}

TEST_CASE("map preserves size", "[lambda]") {
    auto nums = makeList({10, 20, 30});
    auto result = map(nums, [](int n) -> int { return n + 1; });
    CHECK(List::size(result) == 3);
}

TEST_CASE("map captures local variable", "[lambda]") {
    auto nums = makeList({1, 2, 3});
    int factor = 10;
    auto scaled = map(nums, [&](int n) -> int { return n * factor; });
    CHECK(*List::get(scaled, 0) == 10);
    CHECK(*List::get(scaled, 2) == 30);
}

TEST_CASE("map on empty returns empty", "[lambda]") {
    List::It<int> empty;
    auto result = map(empty, [](int n) -> int { return n * 2; });
    CHECK(List::size(result) == 0);
}

TEST_CASE("chained filter then map", "[lambda]") {
    auto nums = makeList({1, 2, 3, 4, 5, 6});
    auto evens  = filter(nums,  [](int n) -> unsigned char { return n % 2 == 0; });
    auto scaled = map(evens, [](int n) -> int { return n * 10; });
    REQUIRE(List::size(scaled) == 3);
    CHECK(*List::get(scaled, 0) == 20);
    CHECK(*List::get(scaled, 1) == 40);
    CHECK(*List::get(scaled, 2) == 60);
}
