#include <catch2/catch_all.hpp>
#include <uhcio.hh>

TEST_CASE("Log all levels smoke test", "[log]") {
    // Verify no crash when calling each log level.
    // Output goes to stdout/stderr; correctness is visual.
    REQUIRE_NOTHROW(Log::trace("trace %d", 1));
    REQUIRE_NOTHROW(Log::debug("debug %d", 2));
    REQUIRE_NOTHROW(Log::info ("info %d",  3));
    REQUIRE_NOTHROW(Log::warn ("warn %d",  4));
    REQUIRE_NOTHROW(Log::error("error %d", 5));
    REQUIRE_NOTHROW(Log::msg  ("msg %d",   6));
}

TEST_CASE("Log format string with multiple args", "[log]") {
    REQUIRE_NOTHROW(Log::info("x=%d y=%f s=%s", 42, 3.14f, "hello"));
}
