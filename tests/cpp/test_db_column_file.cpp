// Phase A. Remove "[.todo]" from one TEST_CASE at a time, top to bottom.

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <score/core/exceptions.hpp>
#include <score/db/column_file.hpp>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include "db_test_util.hpp"

using namespace score::db;
using score::db::testing::read_raw;
using score::db::testing::TempDir;

TEST_CASE("value_size", "[.todo][db][A][column_file]") {
    REQUIRE(value_size(ColumnType::Int32) == 4);
    REQUIRE(value_size(ColumnType::Int64) == 8);
    REQUIRE(value_size(ColumnType::Float64) == 8);
}

TEST_CASE("encode_header: exact bytes", "[.todo][db][A][column_file]") {
    const auto bytes = encode_header(ColumnHeader{kColumnFormatVersion, ColumnType::Float64});
    const std::array<std::uint8_t, kColumnHeaderSize> expected = {'S', 'C', 'O', 'L', 1, 0, 3, 0,
                                                                  0,   0,   0,   0,   0, 0, 0, 0};
    REQUIRE(bytes == expected);
}

TEST_CASE("header: round trip for every type", "[.todo][db][A][column_file]") {
    for (ColumnType type : {ColumnType::Int32, ColumnType::Int64, ColumnType::Float64}) {
        const ColumnHeader header{kColumnFormatVersion, type};
        REQUIRE(decode_header(encode_header(header)) == header);
    }
}

TEST_CASE("decode_header: corrupt headers are rejected", "[.todo][db][A][column_file]") {
    const auto good = encode_header(ColumnHeader{kColumnFormatVersion, ColumnType::Int64});
    REQUIRE(decode_header(good).type == ColumnType::Int64);

    auto bad_magic = good;
    bad_magic[0]   = 'X';
    REQUIRE_THROWS_AS(decode_header(bad_magic), score::DomainError);

    auto bad_version = good;
    bad_version[4]   = 2;
    REQUIRE_THROWS_AS(decode_header(bad_version), score::DomainError);

    auto bad_type = good;
    bad_type[6]   = 0;
    REQUIRE_THROWS_AS(decode_header(bad_type), score::DomainError);
    bad_type[6] = 9;
    REQUIRE_THROWS_AS(decode_header(bad_type), score::DomainError);

    auto bad_reserved = good;
    bad_reserved[15]  = 1;
    REQUIRE_THROWS_AS(decode_header(bad_reserved), score::DomainError);
}

TEST_CASE("open_column_file: creates a file holding only a header", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "close.col";
    {
        UniqueFd fd = open_column_file(path, ColumnType::Float64, true);
        REQUIRE(fd.valid());
    }
    REQUIRE(std::filesystem::file_size(path) == kColumnHeaderSize);
    REQUIRE(read_raw(path).substr(0, 4) == "SCOL");
}

TEST_CASE("open_column_file: reopening checks the type", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "close.col";
    { UniqueFd fd = open_column_file(path, ColumnType::Float64, true); }

    REQUIRE(open_column_file(path, ColumnType::Float64, true).valid());
    REQUIRE(open_column_file(path, ColumnType::Float64, false).valid());
    REQUIRE_THROWS_AS(open_column_file(path, ColumnType::Int64, false), score::DomainError);
    REQUIRE(std::filesystem::file_size(path) == kColumnHeaderSize);  // reopening must not rewrite or grow it
}

TEST_CASE("open_column_file: read-only open of a missing file fails", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "missing.col";
    REQUIRE_THROWS_AS(open_column_file(path, ColumnType::Float64, false), std::system_error);
    REQUIRE_FALSE(std::filesystem::exists(path));
}

TEST_CASE("open_column_file: a file that is not a column file is rejected", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "junk.col";
    score::db::testing::append_raw(path, "this is sixteen b");
    REQUIRE_THROWS_AS(open_column_file(path, ColumnType::Float64, false), score::DomainError);
}

TEST_CASE("write_rows / read_rows: round trip", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "close.col";
    UniqueFd   fd   = open_column_file(path, ColumnType::Float64, true);

    const std::vector<double> values = {1.5, -2.25, 3.0, 1e300, 0.0};
    write_rows(fd.get(), ColumnType::Float64, 0, std::as_bytes(std::span(values)));
    REQUIRE(std::filesystem::file_size(path) == kColumnHeaderSize + values.size() * 8);

    const auto          bytes = read_rows(fd.get(), ColumnType::Float64, 0, values.size());
    std::vector<double> back(values.size());
    std::memcpy(back.data(), bytes.data(), bytes.size());
    REQUIRE(back == values);
}

TEST_CASE("write_rows / read_rows: rows are addressed by index", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "volume.col";
    UniqueFd   fd   = open_column_file(path, ColumnType::Int64, true);

    const std::vector<std::int64_t> first  = {10, 11, 12};
    const std::vector<std::int64_t> second = {13, 14};
    write_rows(fd.get(), ColumnType::Int64, 0, std::as_bytes(std::span(first)));
    write_rows(fd.get(), ColumnType::Int64, 3, std::as_bytes(std::span(second)));

    const auto                bytes = read_rows(fd.get(), ColumnType::Int64, 2, 3);
    std::vector<std::int64_t> back(3);
    std::memcpy(back.data(), bytes.data(), bytes.size());
    REQUIRE(back == std::vector<std::int64_t>{12, 13, 14});
}

TEST_CASE("write_rows: values are little-endian on disk", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "date.col";
    {
        UniqueFd                        fd     = open_column_file(path, ColumnType::Int32, true);
        const std::vector<std::int32_t> values = {0x01020304};
        write_rows(fd.get(), ColumnType::Int32, 0, std::as_bytes(std::span(values)));
    }
    const std::string raw = read_raw(path);
    REQUIRE(raw.size() == kColumnHeaderSize + 4);
    REQUIRE(raw.substr(kColumnHeaderSize) == std::string("\x04\x03\x02\x01", 4));
}

TEST_CASE("write_rows / read_rows: bad sizes are rejected", "[.todo][db][A][column_file]") {
    TempDir    tmp;
    const auto path = tmp.path() / "close.col";
    UniqueFd   fd   = open_column_file(path, ColumnType::Float64, true);

    const std::array<std::byte, 7> seven_bytes{};  // not a whole number of doubles
    REQUIRE_THROWS_AS(write_rows(fd.get(), ColumnType::Float64, 0, seven_bytes), score::DomainError);

    const std::vector<double> values = {1.0, 2.0};
    write_rows(fd.get(), ColumnType::Float64, 0, std::as_bytes(std::span(values)));
    REQUIRE_THROWS_AS(read_rows(fd.get(), ColumnType::Float64, 1, 2), score::DomainError);  // only 1 row left
    REQUIRE(read_rows(fd.get(), ColumnType::Float64, 2, 0).empty());
}

TEST_CASE("is_valid_symbol", "[.todo][db][A][column_file]") {
    for (const char* ok : {"AAPL", "BRK.B", "BF-B", "^GSPC", "EURUSD=X", "a", "0700.HK"}) {
        INFO("symbol: \"" << ok << "\"");
        REQUIRE(is_valid_symbol(ok));
    }
    for (const char* bad : {"", ".", "..", ".hidden", "a/b", "../etc", "a\\b", "A B", "AAPL\n", "é"}) {
        INFO("symbol: \"" << bad << "\"");
        REQUIRE_FALSE(is_valid_symbol(bad));
    }
    REQUIRE(is_valid_symbol(std::string(32, 'A')));
    REQUIRE_FALSE(is_valid_symbol(std::string(33, 'A')));
    REQUIRE_FALSE(is_valid_symbol(std::string("AA\0PL", 5)));
}
