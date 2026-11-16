#pragma once

#include <cstdint>
#include <string>

namespace score::db {

/// One daily OHLCV row for a single symbol.
struct Bar {
    std::string  date;  // ISO-8601 "YYYY-MM-DD"
    double       open;
    double       high;
    double       low;
    double       close;
    std::int64_t volume;
};

}  // namespace score::db
