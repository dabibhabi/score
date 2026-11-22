// ColumnStoreV1 end to end: Phase B (append, count, symbols), Phase C (scan,
// scan_column) and Phase D (several processes). Remove "[.todo]" from one
// TEST_CASE at a time, top to bottom, after the helper tests of the same phase pass.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <score/core/exceptions.hpp>
#include <score/db/bar.hpp>
#include <score/db/column_store_v1.hpp>
#include <score/db/symbol_dir.hpp>
#include <string>
#include <vector>

#include "db_test_util.hpp"

using namespace score::db;
using namespace score::db::testing;

// --- Phase B: append, count, symbols -----------------------------------------

TEST_CASE("ColumnStoreV1: an empty store", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    REQUIRE(store.symbols().empty());
    REQUIRE(store.count("AAPL") == 0);
}

TEST_CASE("ColumnStoreV1: append then count", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL",
                 {{"2023-01-01", 100.0, 110.0, 95.0, 105.0, 1000}, {"2023-01-02", 105.0, 115.0, 100.0, 110.0, 1200}});
    REQUIRE(store.count("AAPL") == 2);
    REQUIRE(store.count("MSFT") == 0);
}

TEST_CASE("ColumnStoreV1: symbols are listed in sorted order", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("MSFT", test_bars(0, 3));
    store.append("AAPL", test_bars(0, 3));
    store.append("BRK.B", test_bars(0, 3));
    REQUIRE(store.symbols() == std::vector<std::string>{"AAPL", "BRK.B", "MSFT"});
}

TEST_CASE("ColumnStoreV1: on-disk layout", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 7));

    const auto dir = tmp.path() / "AAPL";
    REQUIRE(std::filesystem::file_size(dir / "date.col") == kColumnHeaderSize + 7 * 4);
    for (const char* name : {"open.col", "high.col", "low.col", "close.col", "volume.col"}) {
        INFO(name);
        REQUIRE(std::filesystem::file_size(dir / name) == kColumnHeaderSize + 7 * 8);
    }
    REQUIRE(read_committed_count(dir) == 7);
    REQUIRE_FALSE(std::filesystem::exists(dir / kCountTmpFileName));
}

TEST_CASE("ColumnStoreV1: a second append continues the first", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 5));
    store.append("AAPL", test_bars(5, 3));
    REQUIRE(store.count("AAPL") == 8);
}

TEST_CASE("ColumnStoreV1: appending nothing changes nothing", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", {});
    REQUIRE(store.count("AAPL") == 0);
    REQUIRE(store.symbols().empty());
}

TEST_CASE("ColumnStoreV1: data survives reopening the store", "[db][B][columnstore]") {
    TempDir tmp;
    { ColumnStoreV1(tmp.path()).append("AAPL", test_bars(0, 4)); }
    ColumnStoreV1 reopened(tmp.path());
    REQUIRE(reopened.count("AAPL") == 4);
    REQUIRE(reopened.symbols() == std::vector<std::string>{"AAPL"});
}

TEST_CASE("ColumnStoreV1: out-of-order and overlapping bars are rejected", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(10, 5));  // bars 10..14

    REQUIRE_THROWS_AS(store.append("AAPL", test_bars(14, 1)), score::DomainError);  // same date as the last bar
    REQUIRE_THROWS_AS(store.append("AAPL", test_bars(3, 2)), score::DomainError);   // older than what is stored
    REQUIRE_THROWS_AS(store.append("AAPL", {test_bar(20), test_bar(19)}), score::DomainError);  // batch not sorted
    REQUIRE_THROWS_AS(store.append("AAPL", {test_bar(20), test_bar(20)}), score::DomainError);  // duplicate in batch
    REQUIRE(store.count("AAPL") == 5);  // every rejected append left the store untouched

    store.append("AAPL", test_bars(15, 1));
    REQUIRE(store.count("AAPL") == 6);
}

TEST_CASE("ColumnStoreV1: a bad date rejects the whole append", "[db][B][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 2));

    std::vector<Bar> bars = test_bars(2, 3);
    bars[2].date          = "2023-02-30";
    REQUIRE_THROWS_AS(store.append("AAPL", bars), score::DomainError);
    REQUIRE(store.count("AAPL") == 2);
}

TEST_CASE("ColumnStoreV1: unsafe symbol names are rejected", "[db][B][columnstore]") {
    TempDir       outer;
    const auto    root = outer.path() / "store";
    ColumnStoreV1 store(root);

    for (const char* symbol : {"", "..", "../escaped", "a/b", ".hidden"}) {
        INFO("symbol: \"" << symbol << "\"");
        REQUIRE_THROWS_AS(store.append(symbol, test_bars(0, 1)), score::DomainError);
        REQUIRE_THROWS_AS(store.count(symbol), score::DomainError);
    }
    REQUIRE_FALSE(std::filesystem::exists(outer.path() / "escaped"));
    REQUIRE(store.symbols().empty());
}

TEST_CASE("ColumnStoreV1: recovery from an append that crashed half way", "[db][B][columnstore]") {
    TempDir    tmp;
    const auto dir = tmp.path() / "AAPL";
    { ColumnStoreV1(tmp.path()).append("AAPL", test_bars(0, 5)); }

    // Fake the crash: two columns received rows, the others did not, and the
    // writer died before renaming COUNT.tmp. COUNT still says 5.
    append_raw(dir / "date.col", std::string(8, '\x7f'));
    append_raw(dir / "close.col", std::string(16, '\x7f'));
    append_raw(dir / kCountTmpFileName, std::string("\x07\x00\x00\x00\x00\x00\x00\x00", 8));

    ColumnStoreV1 store(tmp.path());
    REQUIRE(store.count("AAPL") == 5);  // readers trust COUNT, not file sizes

    store.append("AAPL", test_bars(5, 2));  // the next writer cleans up, then appends
    REQUIRE(store.count("AAPL") == 7);
    REQUIRE(std::filesystem::file_size(dir / "date.col") == kColumnHeaderSize + 7 * 4);
    REQUIRE(std::filesystem::file_size(dir / "close.col") == kColumnHeaderSize + 7 * 8);
}

TEST_CASE("ColumnStoreV1: a symbol whose first append never committed does not exist", "[db][B][columnstore]") {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path() / "GHOST");  // directory made, crash before COUNT
    ColumnStoreV1 store(tmp.path());
    REQUIRE(store.symbols().empty());
    REQUIRE(store.count("GHOST") == 0);
}

// --- Phase C: scan, scan_column ----------------------------------------------

TEST_CASE("ColumnStoreV1::scan: returns exactly what was appended", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    const auto    bars = test_bars(0, 40);
    store.append("AAPL", bars);

    const auto got = store.scan("AAPL", "0001-01-01", "9999-12-31");
    REQUIRE(got.size() == bars.size());
    for (std::size_t i = 0; i < bars.size(); ++i) {
        INFO("row " << i);
        REQUIRE(same_bar(got[i], bars[i]));
    }
}

TEST_CASE("ColumnStoreV1::scan: both ends of the range are inclusive", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 40));

    const auto got = store.scan("AAPL", test_date(10), test_date(19));
    REQUIRE(got.size() == 10);
    REQUIRE(same_bar(got.front(), test_bar(10)));
    REQUIRE(same_bar(got.back(), test_bar(19)));

    REQUIRE(store.scan("AAPL", test_date(7), test_date(7)).size() == 1);
}

TEST_CASE("ColumnStoreV1::scan: range ends need not be stored dates", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    // Stored: 2023-01-02, -04, -06 (gaps on purpose, like weekends).
    store.append("AAPL", {{"2023-01-02", 1, 1, 1, 1, 1}, {"2023-01-04", 2, 2, 2, 2, 2}, {"2023-01-06", 3, 3, 3, 3, 3}});

    REQUIRE(store.scan("AAPL", "2023-01-03", "2023-01-05").size() == 1);
    REQUIRE(store.scan("AAPL", "2023-01-03", "2023-01-03").empty());
    REQUIRE(store.scan("AAPL", "2022-01-01", "2023-01-02").size() == 1);
    REQUIRE(store.scan("AAPL", "2023-01-06", "2030-01-01").size() == 1);
    REQUIRE(store.scan("AAPL", "2020-01-01", "2022-12-31").empty());
    REQUIRE(store.scan("AAPL", "2023-01-07", "2030-01-01").empty());
    REQUIRE(store.scan("AAPL", "2023-01-06", "2023-01-02").empty());  // start after end
}

TEST_CASE("ColumnStoreV1::scan: unknown symbol and bad arguments", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 3));

    REQUIRE(store.scan("MSFT", "2000-01-01", "2030-01-01").empty());
    REQUIRE_THROWS_AS(store.scan("AAPL", "not-a-date", "2030-01-01"), score::DomainError);
    REQUIRE_THROWS_AS(store.scan("../AAPL", "2000-01-01", "2030-01-01"), score::DomainError);
}

TEST_CASE("ColumnStoreV1::scan: spans two appends", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 5));
    store.append("AAPL", test_bars(5, 5));

    const auto got = store.scan("AAPL", test_date(3), test_date(6));
    REQUIRE(got.size() == 4);
    for (std::size_t i = 0; i < got.size(); ++i) { REQUIRE(same_bar(got[i], test_bar(3 + i))); }
}

TEST_CASE("ColumnStoreV1::scan: never returns rows from a crashed append", "[db][C][columnstore]") {
    TempDir    tmp;
    const auto dir = tmp.path() / "AAPL";
    { ColumnStoreV1(tmp.path()).append("AAPL", test_bars(0, 5)); }
    for (const char* name : {"open.col", "high.col", "low.col", "close.col", "volume.col"}) {
        append_raw(dir / name, std::string(16, '\x01'));
    }
    append_raw(dir / "date.col", std::string(8, '\x7f'));  // two huge, uncommitted dates

    ColumnStoreV1 store(tmp.path());
    const auto    got = store.scan("AAPL", "0001-01-01", "9999-12-31");
    REQUIRE(got.size() == 5);
    REQUIRE(same_bar(got.back(), test_bar(4)));
}

TEST_CASE("ColumnStoreV1::scan_column: one column, no row structs", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 30));

    const ColumnView closes = store.scan_column("AAPL", Column::Close, test_date(10), test_date(14));
    REQUIRE(closes.type() == ColumnType::Float64);
    REQUIRE(closes.size() == 5);
    for (std::size_t i = 0; i < 5; ++i) { REQUIRE(closes.f64()[i] == test_bar(10 + i).close); }

    const ColumnView volumes = store.scan_column("AAPL", Column::Volume, test_date(0), test_date(1));
    REQUIRE(volumes.i64().size() == 2);
    REQUIRE(volumes.i64()[1] == test_bar(1).volume);

    const ColumnView dates = store.scan_column("AAPL", Column::Date, test_date(29), test_date(29));
    REQUIRE(dates.i32().size() == 1);

    REQUIRE(store.scan_column("AAPL", Column::Open, "2099-01-01", "2099-12-31").empty());
    REQUIRE(store.scan_column("MSFT", Column::Open, "2000-01-01", "2099-12-31").empty());
}

TEST_CASE("ColumnStoreV1::scan_column: a view outlives later appends", "[db][C][columnstore]") {
    TempDir       tmp;
    ColumnStoreV1 store(tmp.path());
    store.append("AAPL", test_bars(0, 10));

    const ColumnView before = store.scan_column("AAPL", Column::Close, "0001-01-01", "9999-12-31");
    store.append("AAPL", test_bars(10, 5000));

    REQUIRE(before.size() == 10);  // the snapshot it was taken at
    for (std::size_t i = 0; i < 10; ++i) { REQUIRE(before.f64()[i] == test_bar(i).close); }
    REQUIRE(store.scan_column("AAPL", Column::Close, "0001-01-01", "9999-12-31").size() == 5010);
}

// --- Phase D: several processes ----------------------------------------------

TEST_CASE("ColumnStoreV1: processes appending different symbols in parallel", "[db][D][columnstore]") {
    TempDir                        tmp;
    const std::vector<std::string> symbols = {"AAPL", "MSFT", "GOOG", "AMZN"};

    std::vector<pid_t> children;
    for (const auto& symbol : symbols) {
        children.push_back(run_in_child([&] {
            ColumnStoreV1 store(tmp.path());
            for (std::size_t batch = 0; batch < 20; ++batch) { store.append(symbol, test_bars(batch * 10, 10)); }
        }));
    }
    for (pid_t pid : children) { REQUIRE(wait_for_child(pid) == kChildOk); }

    ColumnStoreV1 store(tmp.path());
    REQUIRE(store.symbols().size() == symbols.size());
    for (const auto& symbol : symbols) {
        INFO(symbol);
        const auto got = store.scan(symbol, "0001-01-01", "9999-12-31");
        REQUIRE(got.size() == 200);
        REQUIRE(std::ranges::all_of(got,
                                    [i = std::size_t{0}](const Bar& b) mutable { return same_bar(b, test_bar(i++)); }));
    }
}

TEST_CASE("ColumnStoreV1: two processes appending the same bars to one symbol", "[db][D][columnstore]") {
    // Both read "no rows yet" unless the lock makes them take turns. With the
    // lock, the second one sees the first one's rows and is rejected as an overlap.
    for (int round = 0; round < 20; ++round) {
        TempDir tmp;
        auto    writer = [&] { ColumnStoreV1(tmp.path()).append("AAPL", test_bars(0, 500)); };

        const pid_t a      = run_in_child(writer);
        const pid_t b      = run_in_child(writer);
        const int   code_a = wait_for_child(a);
        const int   code_b = wait_for_child(b);

        INFO("round " << round << ": exit codes " << code_a << " and " << code_b);
        REQUIRE(((code_a == kChildOk && code_b == kChildDomainError) ||
                 (code_a == kChildDomainError && code_b == kChildOk)));

        ColumnStoreV1 store(tmp.path());
        const auto    got = store.scan("AAPL", "0001-01-01", "9999-12-31");
        REQUIRE(got.size() == 500);
        REQUIRE(same_bar(got.back(), test_bar(499)));
    }
}

TEST_CASE("ColumnStoreV1: a reader never sees part of an append", "[db][D][columnstore]") {
    TempDir               tmp;
    constexpr std::size_t kBatch   = 250;
    constexpr std::size_t kBatches = 40;

    const pid_t writer = run_in_child([&] {
        ColumnStoreV1 store(tmp.path());
        for (std::size_t batch = 0; batch < kBatches; ++batch) {
            store.append("AAPL", test_bars(batch * kBatch, kBatch));
        }
    });

    ColumnStoreV1 store(tmp.path());
    std::size_t   scans       = 0;
    std::size_t   seen        = 0;
    int           writer_code = -1;
    bool          writer_done = false;
    while (seen < kBatch * kBatches) {
        // Check the writer first: a scan started after it exited sees everything.
        if (!writer_done) { writer_done = child_has_exited(writer, writer_code); }
        const bool last_scan = writer_done;

        const auto got = store.scan("AAPL", "0001-01-01", "9999-12-31");
        ++scans;
        REQUIRE(got.size() % kBatch == 0);  // whole appends only
        REQUIRE(got.size() >= seen);        // committed rows never disappear
        seen = got.size();
        for (std::size_t i = 0; i < got.size(); ++i) {
            if (!same_bar(got[i], test_bar(i))) {
                FAIL("row " << i << " is torn or misaligned after " << scans << " scans");
            }
        }
        if (last_scan) { break; }
    }
    if (!writer_done) { writer_code = wait_for_child(writer); }
    REQUIRE(writer_code == kChildOk);
    REQUIRE(seen == kBatch * kBatches);
    REQUIRE(store.count("AAPL") == kBatch * kBatches);
}
