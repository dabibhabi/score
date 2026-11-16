#include <score/core/exceptions.hpp>
#include <score/db/column_store_v1.hpp>
#include <score/db/date_codec.hpp>
#include <score/db/symbol_dir.hpp>
#include <utility>

namespace score::db {

// This file is glue: every hard part lives in a helper you have already
// written and tested by the time you get here (date_codec, column_file,
// symbol_dir, column_view). Do those first; see tasks/todo.md for the order.

ColumnStoreV1::ColumnStoreV1(std::filesystem::path root_dir) : root_dir_(std::move(root_dir)) {
    std::filesystem::create_directories(root_dir_);
}

std::filesystem::path ColumnStoreV1::symbol_dir(const std::string& symbol) const {
    // TODO(B6): throw score::DomainError unless is_valid_symbol(symbol); return root_dir_ / symbol.
    (void)symbol;
    throw score::NotImplementedError("score::db::ColumnStoreV1::symbol_dir");
}

void ColumnStoreV1::append(const std::string& symbol, const std::vector<Bar>& bars) {
    // TODO(B7): the write path. Steps 2-8 run while holding the lock.
    //  0. Empty `bars`: return. Convert every bar.date with days_from_iso first,
    //     and reject bars that are not strictly increasing among themselves
    //     (score::DomainError), before touching the disk.
    //  1. dir = symbol_dir(symbol); std::filesystem::create_directories(dir).
    //  2. SymbolLock lock(dir).                        (Phase D; skip until then)
    //  3. n = read_committed_count(dir).
    //  4. Open the six column files writable (open_column_file).
    //  5. truncate_to_rows(fd, type, n) on each: removes rows left by a crashed append.
    //  6. If n > 0: read the last committed date (read_rows on date.col, row n - 1)
    //     and reject the append unless bars.front() is strictly after it.
    //  7. Build one contiguous array per column and write_rows(..., first_row = n, ...),
    //     then full_fsync each file.
    //  8. commit_count(dir, n + bars.size()).           <- the append becomes visible here
    //
    // Before step 8 a crash loses the append but nothing else. After it, the
    // append is durable. There is no state in between.
    (void)symbol;
    (void)bars;
    throw score::NotImplementedError("score::db::ColumnStoreV1::append");
}

ColumnStoreV1::RowRange ColumnStoreV1::find_rows(const std::filesystem::path& dir, std::size_t committed,
                                                 const std::string& start_date, const std::string& end_date) const {
    // TODO(C6):
    //  1. committed == 0 or start_date > end_date: return {}.
    //  2. Map the first `committed` rows of date.col (ColumnView::map(...).i32()).
    //  3. first = std::lower_bound(dates, days_from_iso(start_date))
    //     last  = std::upper_bound(dates, days_from_iso(end_date))
    //  4. Return {first index, last - first}.
    (void)dir;
    (void)committed;
    (void)start_date;
    (void)end_date;
    throw score::NotImplementedError("score::db::ColumnStoreV1::find_rows");
}

std::vector<Bar> ColumnStoreV1::scan(const std::string& symbol, const std::string& start_date,
                                     const std::string& end_date) const {
    // TODO(C7):
    //  1. dir = symbol_dir(symbol); unknown symbol (no directory): return {}.
    //  2. committed = read_committed_count(dir)   <- the reader's snapshot; read it once.
    //  3. range = find_rows(dir, committed, start_date, end_date).
    //  4. Map the six columns over that range and zip row i into a Bar
    //     (iso_from_days for the date).
    (void)symbol;
    (void)start_date;
    (void)end_date;
    throw score::NotImplementedError("score::db::ColumnStoreV1::scan");
}

ColumnView ColumnStoreV1::scan_column(const std::string& symbol, Column column, const std::string& start_date,
                                      const std::string& end_date) const {
    // TODO(C8): steps 1-3 of scan(), then return ColumnView::map for the one
    // requested file. A small table from Column to {file name, ColumnType}
    // serves this function, scan() and append().
    (void)symbol;
    (void)column;
    (void)start_date;
    (void)end_date;
    throw score::NotImplementedError("score::db::ColumnStoreV1::scan_column");
}

std::vector<std::string> ColumnStoreV1::symbols() const {
    // TODO(B8): every subdirectory of root_dir_ whose read_committed_count > 0,
    // sorted. A directory with no COUNT is a symbol whose first append never
    // committed: it does not exist yet as far as readers are concerned.
    throw score::NotImplementedError("score::db::ColumnStoreV1::symbols");
}

std::size_t ColumnStoreV1::count(const std::string& symbol) const {
    // TODO(B9): read_committed_count(symbol_dir(symbol)); 0 for an unknown symbol.
    (void)symbol;
    throw score::NotImplementedError("score::db::ColumnStoreV1::count");
}

}  // namespace score::db
