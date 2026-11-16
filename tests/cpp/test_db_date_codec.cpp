// Phase A. Remove "[.todo]" from one TEST_CASE at a time, top to bottom.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iterator>
#include <score/core/exceptions.hpp>
#include <score/db/date_codec.hpp>
#include <string>

using score::db::days_from_iso;
using score::db::iso_from_days;

TEST_CASE("days_from_iso: the epoch and its neighbours", "[.todo][db][A][date]") {
    REQUIRE(days_from_iso("1970-01-01") == 0);
    REQUIRE(days_from_iso("1970-01-02") == 1);
    REQUIRE(days_from_iso("1969-12-31") == -1);
}

TEST_CASE("days_from_iso: known dates, including leap days", "[.todo][db][A][date]") {
    REQUIRE(days_from_iso("2000-03-01") == 11017);  // 2000 is a leap year (divisible by 400)
    REQUIRE(days_from_iso("2024-02-29") == 19782);
    REQUIRE(days_from_iso("2023-01-01") == 19358);
    REQUIRE(days_from_iso("1900-01-01") == -25567);  // 1900 is not a leap year
    REQUIRE(days_from_iso("2100-12-31") == 47846);
}

TEST_CASE("iso_from_days: zero-padded output", "[.todo][db][A][date]") {
    REQUIRE(iso_from_days(0) == "1970-01-01");
    REQUIRE(iso_from_days(19782) == "2024-02-29");
    REQUIRE(iso_from_days(-1) == "1969-12-31");
}

TEST_CASE("date codec: round trip over 80 years", "[.todo][db][A][date]") {
    for (std::int32_t d = -1000; d < 30000; d += 37) { REQUIRE(days_from_iso(iso_from_days(d)) == d); }
}

TEST_CASE("date codec: integer order matches string order", "[.todo][db][A][date]") {
    const std::string dates[] = {"1999-12-31", "2000-01-01", "2000-02-28", "2000-02-29", "2000-03-01", "2001-01-01"};
    for (std::size_t i = 1; i < std::size(dates); ++i) {
        REQUIRE(days_from_iso(dates[i - 1]) < days_from_iso(dates[i]));
    }
}

TEST_CASE("days_from_iso: malformed or impossible dates are rejected", "[.todo][db][A][date]") {
    const char* bad[] = {
        "",           "2023-1-01",  "2023/01/01", "20230101",   "2023-01-011", "abcd-ef-gh", " 2023-01-01",
        "2023-13-01", "2023-00-10", "2023-01-00", "2023-01-32", "2023-02-29",  "2023-02-30", "2023-04-31",
    };
    for (const char* s : bad) {
        INFO("input: \"" << s << "\"");
        REQUIRE_THROWS_AS(days_from_iso(s), score::DomainError);
    }
}
