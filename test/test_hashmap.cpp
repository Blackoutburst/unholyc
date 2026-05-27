#include <catch2/catch_all.hpp>
#include <uhcstd.hh>
#include <stdexcept>

TEST_CASE("HashMap empty state", "[hashmap]") {
    HashMap::It<std::string, int> m;
    CHECK(HashMap::size(m) == 0);
    CHECK(HashMap::isEmpty(m) != 0);
}

TEST_CASE("HashMap::put and size", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("a"), 1);
    HashMap::put(m, std::string("b"), 2);
    HashMap::put(m, std::string("c"), 3);
    CHECK(HashMap::size(m) == 3);
    CHECK(HashMap::isEmpty(m) == 0);
}

TEST_CASE("HashMap::put overwrites existing key", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("key"), 10);
    HashMap::put(m, std::string("key"), 99);
    CHECK(HashMap::size(m) == 1);
    CHECK(HashMap::get(m, std::string("key")) == 99);
}

TEST_CASE("HashMap::get found", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("x"), 42);
    int& v = HashMap::get(m, std::string("x"));
    CHECK(v == 42);
}

TEST_CASE("HashMap::get not found throws", "[hashmap]") {
    HashMap::It<std::string, int> m;
    REQUIRE_THROWS_AS(HashMap::get(m, std::string("missing")), std::out_of_range);
}

TEST_CASE("HashMap::getOrDefault found", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("k"), 7);
    CHECK(HashMap::getOrDefault(m, std::string("k"), 0) == 7);
}

TEST_CASE("HashMap::getOrDefault missing returns default", "[hashmap]") {
    HashMap::It<std::string, int> m;
    CHECK(HashMap::getOrDefault(m, std::string("missing"), -1) == -1);
}

TEST_CASE("HashMap::remove present", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("del"), 5);
    unsigned char removed = HashMap::remove(m, std::string("del"));
    CHECK(removed != 0);
    CHECK(HashMap::size(m) == 0);
    CHECK(HashMap::containsKey(m, std::string("del")) == 0);
}

TEST_CASE("HashMap::remove absent", "[hashmap]") {
    HashMap::It<std::string, int> m;
    unsigned char removed = HashMap::remove(m, std::string("nope"));
    CHECK(removed == 0);
}

TEST_CASE("HashMap::containsKey", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("yes"), 1);
    CHECK(HashMap::containsKey(m, std::string("yes")) != 0);
    CHECK(HashMap::containsKey(m, std::string("no"))  == 0);
}

TEST_CASE("HashMap::containsValue", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("a"), 100);
    HashMap::put(m, std::string("b"), 200);
    CHECK(HashMap::containsValue(m, 100) != 0);
    CHECK(HashMap::containsValue(m, 999) == 0);
}

TEST_CASE("HashMap::clear", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("a"), 1);
    HashMap::put(m, std::string("b"), 2);
    HashMap::clear(m);
    CHECK(HashMap::size(m) == 0);
    CHECK(HashMap::isEmpty(m) != 0);
}

TEST_CASE("HashMap::keys", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("x"), 1);
    HashMap::put(m, std::string("y"), 2);
    auto ks = HashMap::keys(m);
    CHECK(List::size(ks) == 2);
}

TEST_CASE("HashMap::values", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("a"), 10);
    HashMap::put(m, std::string("b"), 20);
    auto vs = HashMap::values(m);
    CHECK(List::size(vs) == 2);
}

TEST_CASE("HashMap::putAll merges entries", "[hashmap]") {
    HashMap::It<std::string, int> m1;
    HashMap::put(m1, std::string("a"), 1);
    HashMap::put(m1, std::string("b"), 2);

    HashMap::It<std::string, int> m2;
    HashMap::put(m2, std::string("c"), 3);
    HashMap::put(m2, std::string("d"), 4);

    HashMap::putAll(m1, m2);
    CHECK(HashMap::size(m1) == 4);
    CHECK(HashMap::get(m1, std::string("c")) == 3);
    CHECK(HashMap::get(m1, std::string("d")) == 4);
}

TEST_CASE("HashMap::putAll overwrites on collision", "[hashmap]") {
    HashMap::It<std::string, int> m1;
    HashMap::put(m1, std::string("k"), 1);

    HashMap::It<std::string, int> m2;
    HashMap::put(m2, std::string("k"), 99);

    HashMap::putAll(m1, m2);
    CHECK(HashMap::size(m1) == 1);
    CHECK(HashMap::get(m1, std::string("k")) == 99);
}

TEST_CASE("HashMap::putIfAbsent inserts when absent", "[hashmap]") {
    HashMap::It<std::string, int> m;
    unsigned char inserted = HashMap::putIfAbsent(m, std::string("new"), 42);
    CHECK(inserted != 0);
    CHECK(HashMap::get(m, std::string("new")) == 42);
}

TEST_CASE("HashMap::putIfAbsent skips when present", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("k"), 1);
    unsigned char inserted = HashMap::putIfAbsent(m, std::string("k"), 99);
    CHECK(inserted == 0);
    CHECK(HashMap::get(m, std::string("k")) == 1);
}

TEST_CASE("HashMap::forEach visits all entries", "[hashmap]") {
    HashMap::It<std::string, int> m;
    HashMap::put(m, std::string("a"), 1);
    HashMap::put(m, std::string("b"), 2);
    HashMap::put(m, std::string("c"), 3);

    int sum = 0;
    HashMap::forEach(m, [&](std::string k, int v) {
        sum += v;
    });
    CHECK(sum == 6);
}

TEST_CASE("HashMap works with int keys", "[hashmap]") {
    HashMap::It<int, std::string> m;
    HashMap::put(m, 1, std::string("one"));
    HashMap::put(m, 2, std::string("two"));
    CHECK(HashMap::size(m) == 2);
    CHECK(HashMap::get(m, 1) == std::string("one"));
    CHECK(HashMap::get(m, 2) == std::string("two"));
    CHECK(HashMap::containsKey(m, 99) == 0);
}

TEST_CASE("HashMap works with String keys via std::hash", "[hashmap]") {
    HashMap::It<String::It, int> m;
    HashMap::put(m, String::It("hello"), 1);
    HashMap::put(m, String::It("world"), 2);
    CHECK(HashMap::size(m) == 2);
    CHECK(HashMap::get(m, String::It("hello")) == 1);
    CHECK(HashMap::get(m, String::It("world")) == 2);
    CHECK(HashMap::containsKey(m, String::It("missing")) == 0);
}
