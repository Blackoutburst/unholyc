#include <catch2/catch_all.hpp>
#include <uhcio.hh>
#include <uhcstd.hh>
#include <stdlib.h>
#include <string.h>
#include <string>

static std::string testDir() {
    std::string p = __FILE__;
    auto slash = p.rfind('/');
    return (slash != std::string::npos) ? p.substr(0, slash) : ".";
}

TEST_CASE("File::open non-existent returns empty File", "[file]") {
    String::It p0("/tmp/uhc_test_nonexistent_xyzzy_12345.txt");
    File::It f = File::open(p0);
    CHECK(f.handle == nullptr);
    CHECK(f.size == 0);
}

TEST_CASE("File::open existing sets size and handle", "[file]") {
    std::string path = testDir() + "/test_data.txt";
    String::It p1(path.c_str());
    File::It f = File::open(p1);
    REQUIRE(f.handle != nullptr);
    CHECK(f.size > 0);
    File::close(f);
}

TEST_CASE("File::read returns full content", "[file]") {
    std::string path = testDir() + "/test_data.txt";
    String::It p2(path.c_str());
    File::It f = File::open(p2);
    REQUIRE(f.size > 0);
    String::It content = File::read(f);
    CHECK(!content.handle.empty());
    CHECK(content.handle.find("Hello, UnholyC!") != std::string::npos);
    CHECK(content.handle.find("Line 2") != std::string::npos);
}

TEST_CASE("File::read content length matches size", "[file]") {
    std::string path = testDir() + "/test_data.txt";
    String::It p3(path.c_str());
    File::It f = File::open(p3);
    REQUIRE(f.handle != nullptr);
    long long expected_size = f.size;
    String::It content = File::read(f);
    CHECK((long long)content.handle.size() == expected_size);
}

TEST_CASE("File::read on empty file returns empty string", "[file]") {
    File::It f;
    f.handle = nullptr;
    f.size   = 0;
    String::It result = File::read(f);
    CHECK(result.handle.empty());
}
