#pragma once

#include <filesystem>
#include <score/db/column_store.hpp>

namespace score::db {

/// First concrete ColumnStore: one directory per symbol, one fixed-width file
/// per column, and a committed row count that makes appends atomic.
///
///   root/<SYMBOL>/date.col     Int32    days since 1970-01-01, strictly increasing
///   root/<SYMBOL>/open.col     Float64
///   root/<SYMBOL>/high.col     Float64
///   root/<SYMBOL>/low.col      Float64
///   root/<SYMBOL>/close.col    Float64
///   root/<SYMBOL>/volume.col   Int64
///   root/<SYMBOL>/COUNT        committed row count (see symbol_dir.hpp)
///   root/<SYMBOL>/LOCK         writer lock (see symbol_dir.hpp)
///
/// Column file bytes are specified in column_file.hpp. Row r of every column
/// belongs to the same bar.
///
/// Guarantees once implemented:
///  - Atomic append: after a crash a symbol holds either all of an append's
///    bars or none of them.
///  - Many writers: appends to different symbols run in parallel, from
///    threads or processes; appends to the same symbol queue up on LOCK.
///  - Lock-free readers: a reader takes one snapshot of COUNT and never looks
///    past it, so it cannot see half of an append.
///
/// Not supported in v1: updates, deletes, out-of-order inserts, compression.
class ColumnStoreV1 : public ColumnStore {
public:
    explicit ColumnStoreV1(std::filesystem::path root_dir);

    void                           append(const std::string& symbol, const std::vector<Bar>& bars) override;
    [[nodiscard]] std::vector<Bar> scan(const std::string& symbol, const std::string& start_date,
                                        const std::string& end_date) const override;
    [[nodiscard]] ColumnView       scan_column(const std::string& symbol, Column column, const std::string& start_date,
                                               const std::string& end_date) const override;
    [[nodiscard]] std::vector<std::string> symbols() const override;
    [[nodiscard]] std::size_t              count(const std::string& symbol) const override;

private:
    struct RowRange {
        std::size_t first = 0;  // index of the first row in range
        std::size_t count = 0;  // number of rows in range
    };

    /// root_dir_ / symbol, after checking is_valid_symbol (score::DomainError otherwise).
    [[nodiscard]] std::filesystem::path symbol_dir(const std::string& symbol) const;

    /// Rows of `symbol` with date in [start_date, end_date], looking only at
    /// the first `committed` rows. Shared by scan() and scan_column().
    [[nodiscard]] RowRange find_rows(const std::filesystem::path& dir, std::size_t committed,
                                     const std::string& start_date, const std::string& end_date) const;

    std::filesystem::path root_dir_;
};

}  // namespace score::db
