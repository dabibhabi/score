#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <score/core/exceptions.hpp>
#include <score/db/column_store_v1.hpp>
#include <score/db/date_codec.hpp>
#include <score/db/symbol_dir.hpp>
#include <span>
#include <utility>
#include <vector>

namespace score::db {

// This file is glue: every hard part lives in a helper you have already
// written and tested by the time you get here (date_codec, column_file,
// symbol_dir, column_view). Do those first; see tasks/todo.md for the order.

namespace {

struct ColumnSpec {
    const char* file;
    ColumnType  type;
};

// B7 (and C8 later): file name and type of each column, indexed by static_cast<std::size_t>(Column).
constexpr std::array<ColumnSpec, 6> kColumns = {{
    {"date.col", ColumnType::Int32},
    {"open.col", ColumnType::Float64},
    {"high.col", ColumnType::Float64},
    {"low.col", ColumnType::Float64},
    {"close.col", ColumnType::Float64},
    {"volume.col", ColumnType::Int64},
}};

constexpr std::size_t kDateColumn = static_cast<std::size_t>(Column::Date);

struct ColumnData {
    std::vector<std::int32_t> date;
    std::vector<double>       open, high, low, close;
    std::vector<std::int64_t> volume;
};

// B7 step 0: bars -> six column arrays. A bad date or a date that is not strictly after the one
// before it throws DomainError. Touches no files, so a rejected batch leaves the disk alone.
ColumnData to_columns(const std::vector<Bar>& bars) {
    ColumnData d;
    for(const Bar& b : bars) {
        const std::int32_t day = days_from_iso(b.date);
        if(!d.date.empty() && day <= d.date.back()) {
            throw score::DomainError("score::db::ColumnStoreV1::append: dates must be strictly increasing");
        }
        d.date.push_back(day);
        d.open.push_back(b.open);
        d.high.push_back(b.high);
        d.low.push_back(b.low);
        d.close.push_back(b.close);
        d.volume.push_back(b.volume);
    }
    return d;
}

// B7 step 7: the bytes of column `i` (in kColumns order), ready for write_rows.
std::span<const std::byte> bytes_of(const ColumnData& d, std::size_t i) {
    switch(static_cast<Column>(i)) {
        case Column::Date:   return std::as_bytes(std::span(d.date));
        case Column::Open:   return std::as_bytes(std::span(d.open));
        case Column::High:   return std::as_bytes(std::span(d.high));
        case Column::Low:    return std::as_bytes(std::span(d.low));
        case Column::Close:  return std::as_bytes(std::span(d.close));
        case Column::Volume: return std::as_bytes(std::span(d.volume));
    }
    throw score::DomainError("score::db::ColumnStoreV1: unknown column");
}

// B7 step 4: opens the six column files of `dir` writable (new ones get a header), in kColumns order.
std::array<UniqueFd, 6> open_all_columns(const std::filesystem::path& dir) {
    std::array<UniqueFd, 6> fds;
    for(std::size_t i = 0; i < kColumns.size(); ++i) {
        fds[i] = open_column_file(dir / kColumns[i].file, kColumns[i].type, true);
    }
    return fds;
}

// B7 step 6: the date (days since 1970) in row n - 1 of date.col. Call it only when n > 0.
std::int32_t last_date(int date_fd, std::size_t n) {
    const auto    bytes = read_rows(date_fd, ColumnType::Int32, n - 1, 1);
    std::int32_t day   = 0;
    std::memcpy(&day, bytes.data(), sizeof(day));
    return day;
}

}  // namespace

ColumnStoreV1::ColumnStoreV1(std::filesystem::path root_dir) : root_dir_(std::move(root_dir)) {
    std::filesystem::create_directories(root_dir_);
}

std::filesystem::path ColumnStoreV1::symbol_dir(const std::string& symbol) const {
    if(!is_valid_symbol(symbol)) {
        throw score::DomainError("score::db::ColumnStoreV1: invalid symbol name '" + symbol + "'");
    }
    return root_dir_ / symbol;
}

void ColumnStoreV1::append(const std::string& symbol, const std::vector<Bar>& bars) {
    const auto symbol_path = symbol_dir(symbol);
    if(bars.empty()) { return; }

    const ColumnData new_rows = to_columns(bars);  // bad or unsorted dates throw here, before any file is touched
    std::filesystem::create_directories(symbol_path);

    // Phase D adds the lock here:  SymbolLock lock(symbol_path);
    const std::size_t committed_rows = read_committed_count(symbol_path);
    const auto        column_fds     = open_all_columns(symbol_path);

    const auto for_each_column = [&](auto action) {
        for(std::size_t column_index = 0; column_index < kColumns.size(); ++column_index) {
            action(column_fds[column_index].get(), kColumns[column_index].type, column_index);
        }
    };

    // Removes rows left behind by an append that crashed before its commit. A no-op after a clean shutdown.
    for_each_column([&](int fd, ColumnType type, std::size_t) { truncate_to_rows(fd, type, committed_rows); });

    const int  date_fd               = column_fds[kDateColumn].get();
    const bool overlaps_stored_dates = committed_rows > 0 && new_rows.date.front() <= last_date(date_fd, committed_rows);
    if(overlaps_stored_dates) {
        throw score::DomainError("score::db::ColumnStoreV1::append: bars overlap stored dates");
    }

    for_each_column([&](int fd, ColumnType type, std::size_t column_index) {
        write_rows(fd, type, committed_rows, bytes_of(new_rows, column_index));
        full_fsync(fd);
    });

    commit_count(symbol_path, committed_rows + bars.size());  // the append becomes visible here
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
    // A directory without a COUNT is a symbol whose first append never committed: it does not exist yet.
    const auto has_committed_rows = [](const std::filesystem::directory_entry& entry) {
        return entry.is_directory() && read_committed_count(entry.path()) > 0;
    };

    std::vector<std::string> symbol_names;
    for(const auto& entry : std::filesystem::directory_iterator(root_dir_)) {
        if(has_committed_rows(entry)) { symbol_names.push_back(entry.path().filename().string()); }
    }
    std::sort(symbol_names.begin(), symbol_names.end());
    return symbol_names;
}

std::size_t ColumnStoreV1::count(const std::string& symbol) const {
    return read_committed_count(symbol_dir(symbol));
}

}  // namespace score::db
