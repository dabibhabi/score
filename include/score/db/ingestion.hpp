#pragma once

#include <cstddef>
#include <score/db/column_store.hpp>
#include <string>

namespace score::db {

/// One data producer that can write bars for a symbol into a ColumnStore.
///
/// Eventual Python-side producers: src/score/market.py (live yfinance pulls)
/// and scripts/gen_mock_data.py (synthetic GBM data) are both expected to
/// implement/bridge to this interface once it's exposed via nanobind — no
/// Python wiring exists yet.
class IngestionSource {
public:
    virtual ~IngestionSource() = default;

    /// Short identifying name for this source (e.g. "yfinance", "mock-gbm").
    [[nodiscard]] virtual std::string name() const = 0;

    /// Fetches/generates bars for `symbol` and appends them to `store`.
    /// Returns the number of bars written.
    virtual std::size_t ingest(ColumnStore& store, const std::string& symbol) = 0;
};

}  // namespace score::db
