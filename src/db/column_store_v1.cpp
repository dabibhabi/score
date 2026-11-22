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

// C6: where a date range sits in a column.
struct RowSpan {
    std::size_t first = 0;  // index of the first row in range
    std::size_t count = 0;  // number of rows in range
};

// C6: rows whose day number lies in [first_day, last_day], both ends inclusive.
// `sorted_days` must be ascending. An empty or inverted range gives {0, 0}.
RowSpan rows_between(std::span<const std::int32_t> sorted_days, std::int32_t first_day, std::int32_t last_day) {
    const auto first = std::ranges::lower_bound(sorted_days, first_day);
    const auto last  = std::ranges::upper_bound(sorted_days, last_day);
    if(last <= first) { return {}; }
    return {static_cast<std::size_t>(first - sorted_days.begin()), static_cast<std::size_t>(last - first)};
}

// C6: a query's start and end date as day numbers. An invalid ISO date throws DomainError.
struct DayRange {
    std::int32_t first_day;
    std::int32_t last_day;
};

DayRange to_day_range(const std::string& start_date, const std::string& end_date) {
    return {days_from_iso(start_date), days_from_iso(end_date)};
}

// C6, C7, C8: the file of one column inside a symbol directory, and its type.
std::filesystem::path column_file_path(const std::filesystem::path& symbol_dir, Column column) {
    return symbol_dir / kColumns[static_cast<std::size_t>(column)].file;
}

ColumnType column_type(Column column) { return kColumns[static_cast<std::size_t>(column)].type; }

// C6, C7, C8: maps rows [first_row, first_row + n_rows) of one column.
ColumnView map_column(const std::filesystem::path& symbol_dir, Column column, std::size_t first_row,
                      std::size_t n_rows) {
    return ColumnView::map(column_file_path(symbol_dir, column), column_type(column), first_row, n_rows);
}

// C7: the six columns of one row range, mapped together so that row i of every view is the same bar.
struct BarColumns {
    ColumnView date, open, high, low, close, volume;

    static BarColumns map(const std::filesystem::path& symbol_dir, std::size_t first_row, std::size_t n_rows) {
        return {map_column(symbol_dir, Column::Date, first_row, n_rows),
                map_column(symbol_dir, Column::Open, first_row, n_rows),
                map_column(symbol_dir, Column::High, first_row, n_rows),
                map_column(symbol_dir, Column::Low, first_row, n_rows),
                map_column(symbol_dir, Column::Close, first_row, n_rows),
                map_column(symbol_dir, Column::Volume, first_row, n_rows)};
    }

    [[nodiscard]] std::size_t size() const { return date.size(); }

    [[nodiscard]] Bar bar(std::size_t row) const {
        return Bar{iso_from_days(date.i32()[row]), open.f64()[row], high.f64()[row],
                   low.f64()[row],                 close.f64()[row], volume.i64()[row]};
    }
};

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

    const SymbolLock lock(symbol_path);  // from here on, no other writer can touch this symbol
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
    if(committed == 0) { return {}; }

    const DayRange   days  = to_day_range(start_date, end_date);  // bad dates throw here, before any comparison
    const ColumnView dates = map_column(dir, Column::Date, 0, committed);
    const RowSpan    span  = rows_between(dates.i32(), days.first_day, days.last_day);
    return {span.first, span.count};
}

std::vector<Bar> ColumnStoreV1::scan(const std::string& symbol, const std::string& start_date,
                                     const std::string& end_date) const {
    const auto        symbol_path    = symbol_dir(symbol);
    const std::size_t committed_rows = read_committed_count(symbol_path);  // the reader's snapshot, read once
    const RowRange    range          = find_rows(symbol_path, committed_rows, start_date, end_date);
    if(range.count == 0) { return {}; }

    const BarColumns columns = BarColumns::map(symbol_path, range.first, range.count);
    std::vector<Bar> bars;
    bars.reserve(columns.size());
    for(std::size_t row = 0; row < columns.size(); ++row) { bars.push_back(columns.bar(row)); }
    return bars;
}

ColumnView ColumnStoreV1::scan_column(const std::string& symbol, Column column, const std::string& start_date,
                                      const std::string& end_date) const {
    const auto        symbol_path    = symbol_dir(symbol);
    const std::size_t committed_rows = read_committed_count(symbol_path);
    const RowRange    range          = find_rows(symbol_path, committed_rows, start_date, end_date);
    if(range.count == 0) { return ColumnView{}; }  // no file to map for an unknown symbol

    return map_column(symbol_path, column, range.first, range.count);
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
