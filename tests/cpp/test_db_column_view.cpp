// Phase C. Remove "[.todo]" from one TEST_CASE at a time, top to bottom.
// These tests build their files with open_column_file/write_rows from Phase A.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <score/core/exceptions.hpp>
#include <score/db/column_view.hpp>
#include <span>
#include <utility>
#include <vector>

#include "db_test_util.hpp"

using namespace score::db;
using score::db::testing::TempDir;

namespace {

// Writes the doubles 0.0, 1.0, ..., n - 1 into a new Float64 column file.
std::filesystem::path make_f64_column(const TempDir& tmp, std::size_t n) {
    const auto          path = tmp.path() / "close.col";
    std::vector<double> values(n);
    std::iota(values.begin(), values.end(), 0.0);
    UniqueFd fd = open_column_file(path, ColumnType::Float64, true);
    write_rows(fd.get(), ColumnType::Float64, 0, std::as_bytes(std::span(values)));
    return path;
}

}  // namespace

TEST_CASE("ColumnView: default-constructed is empty", "[db][C][column_view]") {
    ColumnView view;
    REQUIRE(view.empty());
    REQUIRE(view.f64().empty());
}

TEST_CASE("ColumnView::map: whole file", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 5);

    const ColumnView view = ColumnView::map(path, ColumnType::Float64, 0, 5);
    REQUIRE(view.size() == 5);
    REQUIRE(view.type() == ColumnType::Float64);
    const auto values = view.f64();
    REQUIRE(values.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) { REQUIRE(values[i] == static_cast<double>(i)); }
}

TEST_CASE("ColumnView::map: a sub-range", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 10);

    const ColumnView view   = ColumnView::map(path, ColumnType::Float64, 3, 4);
    const auto       values = view.f64();
    REQUIRE(values.size() == 4);
    REQUIRE(values.front() == 3.0);
    REQUIRE(values.back() == 6.0);
}

TEST_CASE("ColumnView::map: a range that starts in the middle of a later page", "[db][C][column_view]") {
    // 100,000 doubles span many pages (4 KiB on x86-64, 16 KiB on Apple silicon).
    // Row 54,321 does not sit on a page boundary, so this fails unless the
    // offset passed to mmap is rounded down and the remainder added back.
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 100'000);

    const ColumnView view   = ColumnView::map(path, ColumnType::Float64, 54'321, 1000);
    const auto       values = view.f64();
    REQUIRE(values.size() == 1000);
    REQUIRE(values.front() == 54'321.0);
    REQUIRE(values.back() == 55'320.0);
}

TEST_CASE("ColumnView::map: zero rows gives an empty view", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 5);

    const ColumnView view = ColumnView::map(path, ColumnType::Float64, 2, 0);
    REQUIRE(view.empty());
    REQUIRE(view.type() == ColumnType::Float64);
    REQUIRE(view.f64().empty());
}

TEST_CASE("ColumnView::map: asking for more rows than the file holds", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 5);
    REQUIRE(ColumnView::map(path, ColumnType::Float64, 0, 5).size() == 5);
    REQUIRE_THROWS_AS(ColumnView::map(path, ColumnType::Float64, 0, 6), score::DomainError);
    REQUIRE_THROWS_AS(ColumnView::map(path, ColumnType::Float64, 5, 1), score::DomainError);
}

TEST_CASE("ColumnView: type mismatches are rejected", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 5);

    REQUIRE_THROWS_AS(ColumnView::map(path, ColumnType::Int64, 0, 5), score::DomainError);

    const ColumnView view = ColumnView::map(path, ColumnType::Float64, 0, 5);
    REQUIRE(view.f64().size() == 5);
    REQUIRE_THROWS_AS(view.i64(), score::DomainError);
    REQUIRE_THROWS_AS(view.i32(), score::DomainError);
}

TEST_CASE("ColumnView: Int32 and Int64 columns", "[db][C][column_view]") {
    TempDir tmp;

    const auto                      dates_path = tmp.path() / "date.col";
    const std::vector<std::int32_t> dates      = {19358, 19359, 19360};
    {
        UniqueFd fd = open_column_file(dates_path, ColumnType::Int32, true);
        write_rows(fd.get(), ColumnType::Int32, 0, std::as_bytes(std::span(dates)));
    }
    const ColumnView date_view = ColumnView::map(dates_path, ColumnType::Int32, 1, 2);
    REQUIRE(date_view.i32().size() == 2);
    REQUIRE(date_view.i32()[0] == 19359);
    REQUIRE(date_view.i32()[1] == 19360);

    const auto                      volume_path = tmp.path() / "volume.col";
    const std::vector<std::int64_t> volumes     = {1'000'000'000'000, -7};
    {
        UniqueFd fd = open_column_file(volume_path, ColumnType::Int64, true);
        write_rows(fd.get(), ColumnType::Int64, 0, std::as_bytes(std::span(volumes)));
    }
    const ColumnView volume_view = ColumnView::map(volume_path, ColumnType::Int64, 0, 2);
    REQUIRE(volume_view.i64()[0] == 1'000'000'000'000);
    REQUIRE(volume_view.i64()[1] == -7);
}

TEST_CASE("ColumnView: moving transfers the mapping", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 5);

    ColumnView a = ColumnView::map(path, ColumnType::Float64, 0, 5);
    ColumnView b = std::move(a);
    REQUIRE(a.empty());  // NOLINT(bugprone-use-after-move): the moved-from state is specified
    REQUIRE(b.size() == 5);
    REQUIRE(b.f64()[4] == 4.0);
}

TEST_CASE("ColumnView: stays valid and unchanged while the file grows", "[db][C][column_view]") {
    TempDir    tmp;
    const auto path = make_f64_column(tmp, 5);

    const ColumnView view = ColumnView::map(path, ColumnType::Float64, 0, 5);
    {
        // What a later append does: more rows after the ones already mapped.
        const std::vector<double> more(50'000, -1.0);
        UniqueFd                  fd = open_column_file(path, ColumnType::Float64, true);
        write_rows(fd.get(), ColumnType::Float64, 5, std::as_bytes(std::span(more)));
    }
    REQUIRE(view.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) { REQUIRE(view.f64()[i] == static_cast<double>(i)); }
}
