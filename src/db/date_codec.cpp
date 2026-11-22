#include <score/core/exceptions.hpp>
#include <score/db/date_codec.hpp>
#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <charconv>

namespace score::db {



namespace {

// Parses exactly [first, last) as unsigned decimal digits; rejects signs, spaces, trailing junk.
int parse_digits(const char* first, const char* last) {
    int value = 0;
    auto res = std::from_chars(first, last, value);
    if (res.ec != std::errc{} || res.ptr != last || *first == '-' || *first == '+') {
        throw score::DomainError("score::db::days_from_iso: invalid ISO date format");
    }
    return value;
}

}  // namespace

std::int32_t days_from_iso(std::string_view iso) {
    if (iso.length() != 10 || iso[4] != '-' || iso[7] != '-') {
        throw score::DomainError("score::db::days_from_iso: invalid ISO date format");
    }
    const char* p = iso.data();
    int year = parse_digits(p, p + 4);
    int month = parse_digits(p + 5, p + 7);
    int day = parse_digits(p + 8, p + 10);
    std::chrono::year_month_day ymd{std::chrono::year(year), std::chrono::month(month), std::chrono::day(day)};
    if (!ymd.ok()) throw score::DomainError("score::db::days_from_iso: invalid date components");
    return std::chrono::sys_days(ymd).time_since_epoch().count();
}

std::string iso_from_days(std::int32_t days) {
    std::chrono::year_month_day ymd{std::chrono::sys_days{std::chrono::days{days}}};
    return std::format("{:04}-{:02}-{:02}", int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day()));
}

}  // namespace score::db
