#pragma once

#include <cstddef>
#include <score/db/bar.hpp>
#include <string>
#include <vector>

namespace score::db {

/// Storage interface for daily OHLCV bars, keyed by symbol. Implementations
/// own how bars are laid out on disk/in memory; this interface only commits
/// to the read/write contract, not the byte layout.
class ColumnStore {
public:
    virtual ~ColumnStore() = default;

    /// Appends `bars` for `symbol`. Implementations should reject bars that
    /// are out of order or overlap dates already stored for that symbol.
    virtual void append(const std::string& symbol, const std::vector<Bar>& bars) = 0;

    /// Returns the bars for `symbol` with date in [start_date, end_date].
    [[nodiscard]] virtual std::vector<Bar> scan(const std::string& symbol, const std::string& start_date,
                                                 const std::string& end_date) const = 0;

    /// Returns every symbol currently stored.
    [[nodiscard]] virtual std::vector<std::string> symbols() const = 0;

    /// Returns the number of bars stored for `symbol` (0 if unknown).
    [[nodiscard]] virtual std::size_t count(const std::string& symbol) const = 0;
};

}  // namespace score::db
