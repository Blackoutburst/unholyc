#include <catch2/catch_all.hpp>
#include <uhcstd.hh>

TEST_CASE("Time::millis returns increasing value", "[time]") {
    unsigned long long t1 = Time::millis();
    Time::sleep(20);
    unsigned long long t2 = Time::millis();
    CHECK(t2 > t1);
}

TEST_CASE("Time::sleep(100) takes at least 90ms", "[time]") {
    unsigned long long before = Time::millis();
    Time::sleep(100);
    unsigned long long after = Time::millis();
    CHECK(after - before >= 90);
}

TEST_CASE("Time::sleep(100) takes less than 1000ms", "[time]") {
    unsigned long long before = Time::millis();
    Time::sleep(100);
    unsigned long long after = Time::millis();
    CHECK(after - before < 1000);
}

TEST_CASE("Time::millis resolution is millisecond-level", "[time]") {
    unsigned long long t1 = Time::millis();
    Time::sleep(50);
    unsigned long long t2 = Time::millis();
    unsigned long long elapsed = t2 - t1;
    // elapsed should be between 40 and 500ms
    CHECK(elapsed >= 40);
    CHECK(elapsed < 500);
}
