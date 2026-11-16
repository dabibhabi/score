#include <score/core/exceptions.hpp>
#include <score/db/date_codec.hpp>

namespace score::db {

// Hints: tasks/code_snippets.md, "Phase A — dates as day counts".

std::int32_t days_from_iso(std::string_view iso) {
    // TODO(A1):
    //  1. Check the shape: length 10, '-' at index 4 and 7, digits elsewhere.
    //  2. Parse year, month, day (std::from_chars avoids locale and allocation).
    //  3. Build std::chrono::year_month_day and reject it if !ok().
    //  4. Convert to std::chrono::sys_days and return the day count.
    (void)iso;
    throw score::NotImplementedError("score::db::days_from_iso");
}

std::string iso_from_days(std::int32_t days) {
    // TODO(A2):
    //  1. Build std::chrono::sys_days from the day count.
    //  2. Convert to std::chrono::year_month_day.
    //  3. Format as zero-padded "YYYY-MM-DD" (std::format with {:04}-{:02}-{:02}).
    (void)days;
    throw score::NotImplementedError("score::db::iso_from_days");
}

}  // namespace score::db
