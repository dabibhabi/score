#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace score::db {

/// Converts an ISO-8601 calendar date "YYYY-MM-DD" to days since 1970-01-01
/// (1970-01-01 -> 0, 1970-01-02 -> 1, 1969-12-31 -> -1).
///
/// Throws score::DomainError if `iso` is not exactly "YYYY-MM-DD" or names a
/// date that does not exist (e.g. "2023-02-30", "2023-13-01").
///
/// Why an int32 day count: it is fixed width (4 bytes instead of a 10-byte
/// string), and it sorts the same way the calendar does, so a date range
/// becomes a binary search over a plain integer array.
[[nodiscard]] std::int32_t days_from_iso(std::string_view iso);

/// Inverse of days_from_iso: iso_from_days(days_from_iso(s)) == s.
[[nodiscard]] std::string iso_from_days(std::int32_t days);

}  // namespace score::db
