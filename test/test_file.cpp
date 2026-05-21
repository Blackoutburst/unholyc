#include <catch2/catch_all.hpp>
#include <uhcio.hh>
#include <stdlib.h>
#include <string.h>
#include <string>

static std::string testDir() {
    std::string p = __FILE__;
    auto slash = p.rfind('/');
    return (slash != std::string::npos) ? p.substr(0, slash) : ".";
}

TEST_CASE("File::open non-existent returns empty File", "[file]") {
    File::It f = File::open("/tmp/uhc_test_nonexistent_xyzzy_12345.txt");
    CHECK(f.handle == nullptr);
    CHECK(f.size == 0);
}

TEST_CASE("File::open existing sets size and handle", "[file]") {
    std::string path = testDir() + "/test_data.txt";
    File::It f = File::open(path.c_str());
    REQUIRE(f.handle != nullptr);
    CHECK(f.size > 0);
    File::close(f);
}

TEST_CASE("File::read returns full content", "[file]") {
    std::string path = testDir() + "/test_data.txt";
    File::It f = File::open(path.c_str());
    REQUIRE(f.size > 0);
    char* content = File::read(f);
    REQUIRE(content != nullptr);
    CHECK(strstr(content, "Hello, UnholyC!") != nullptr);
    CHECK(strstr(content, "Line 2") != nullptr);
    free(content);
}

TEST_CASE("File::read content length matches size", "[file]") {
    std::string path = testDir() + "/test_data.txt";
    File::It f = File::open(path.c_str());
    REQUIRE(f.handle != nullptr);
    long long expected_size = f.size;
    char* content = File::read(f);
    REQUIRE(content != nullptr);
    CHECK((long long)strlen(content) == expected_size);
    free(content);
}

TEST_CASE("File::read on zero-size file returns null", "[file]") {
    File::It f;
    f.handle = nullptr;
    f.size   = 0;
    char* result = File::read(f);
    CHECK(result == nullptr);
}
