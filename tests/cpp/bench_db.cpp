// Phase F benchmarks. Hidden by the [!benchmark] tag; run them with
//
//   ./build/tests/cpp/score_tests "[!benchmark]" --benchmark-samples 20
//
// They only work once Phases A-D are implemented. Raise kSymbols / kDays
// towards the 5,000 x 7,560 design target when the small run looks sane; the
// numbers at this size mostly measure per-call overhead, not the disk.

#include <algorithm>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <score/db/column_store_v1.hpp>
#include <score/db/date_codec.hpp>
#include <score/statistics/risk.hpp>
#include <string>
#include <vector>

#include "db_test_util.hpp"

using namespace score::db;

namespace {

constexpr std::size_t  kSymbols  = 100;
constexpr std::size_t  kDays     = 2520;   // about 10 years of trading days
constexpr std::int32_t kFirstDay = 10957;  // 2000-01-01

std::string symbol_name(std::size_t i) { return "SYM" + std::to_string(i); }

// A deterministic price path: no randomness, so runs are comparable.
std::vector<Bar> make_history(std::size_t seed) {
    std::vector<Bar> bars;
    bars.reserve(kDays);
    double price = 50.0 + static_cast<double>(seed % 100);
    for (std::size_t d = 0; d < kDays; ++d) {
        price *= 1.0 + 0.01 * (static_cast<double>((d * 7919 + seed * 104729) % 201) - 100.0) / 100.0;
        bars.push_back(Bar{iso_from_days(kFirstDay + static_cast<std::int32_t>(d)), price, price * 1.01, price * 0.99,
                           price, static_cast<std::int64_t>(1'000'000 + d)});
    }
    return bars;
}

}  // namespace

TEST_CASE("db benchmarks", "[!benchmark][db][F]") {
    score::db::testing::TempDir tmp;
    ColumnStoreV1               store(tmp.path());
    for (std::size_t s = 0; s < kSymbols; ++s) { store.append(symbol_name(s), make_history(s)); }

    const std::string first_date = iso_from_days(kFirstDay);
    const std::string last_date  = iso_from_days(kFirstDay + static_cast<std::int32_t>(kDays) - 1);
    const std::string year_ago   = iso_from_days(kFirstDay + static_cast<std::int32_t>(kDays) - 252);
    const std::string var_start  = iso_from_days(kFirstDay + static_cast<std::int32_t>(kDays) - 251);

    BENCHMARK_ADVANCED("ingest: one symbol, full history (fsync included)")(Catch::Benchmark::Chronometer meter) {
        static std::size_t next    = 0;
        const auto         history = make_history(1);
        const std::size_t  base    = next;
        next += static_cast<std::size_t>(meter.runs());
        meter.measure(
            [&](int run) { store.append("ING" + std::to_string(base + static_cast<std::size_t>(run)), history); });
    };

    BENCHMARK("scan_column: close, one symbol, last year") {
        return store.scan_column("SYM7", Column::Close, year_ago, last_date).size();
    };

    BENCHMARK("scan_column: close, one symbol, full history, summed") {
        const ColumnView view = store.scan_column("SYM7", Column::Close, first_date, last_date);
        double           sum  = 0.0;
        for (double v : view.f64()) { sum += v; }
        return sum;
    };

    BENCHMARK("scan: row structs, one symbol, full history") {
        return store.scan("SYM7", first_date, last_date).size();
    };

    // The known weak spot of one-directory-per-symbol: a cross-section opens kSymbols files.
    BENCHMARK("scan_column: close, every symbol, one date") {
        double sum = 0.0;
        for (std::size_t s = 0; s < kSymbols; ++s) {
            const ColumnView view = store.scan_column(symbol_name(s), Column::Close, last_date, last_date);
            sum += view.f64()[0];
        }
        return sum;
    };

    BENCHMARK("95% historical VaR over 250 returns, every symbol") {
        double worst = 0.0;
        for (std::size_t s = 0; s < kSymbols; ++s) {
            const ColumnView view = store.scan_column(symbol_name(s), Column::Close, var_start, last_date);
            worst = std::max(worst, risk::historical_var(risk::log_returns(view.f64()), 0.95));
        }
        return worst;
    };
}
