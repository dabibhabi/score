/*
 * src/bindings/nanobindings.cpp
 *
 * nanobind wrapping for the Score library.  Compiled into the
 * _score_native extension module that lives next to the Python package
 * (python/score/).  Once the C++ stubs are filled in, every method here
 * automatically gets the real behavior.
 */

#include <nanobind/make_iterator.h>
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <score/core/exceptions.hpp>
#include <score/core/types.hpp>
#include <score/db/bar.hpp>
#include <score/db/column_store_v1.hpp>
#include <score/metrics/stock_metrics.hpp>
#include <score/statistics/cdf.hpp>
#include <score/statistics/descriptive.hpp>
#include <score/statistics/risk.hpp>

namespace nb = nanobind;
using namespace nb::literals;

NB_MODULE(_score_native, m) {
    m.doc() = "Score: native C/C++ extensions (statistics + stock metrics)";

    // ----------------------------------------------------------------------
    // Exception translation
    // ----------------------------------------------------------------------
    nb::exception<score::EmptySeriesError>(m, "EmptySeriesError");
    nb::exception<score::DimensionMismatchError>(m, "DimensionMismatchError");
    nb::exception<score::DomainError>(m, "DomainError");
    nb::exception<score::NotImplementedError>(m, "NativeNotImplementedError");

    // ----------------------------------------------------------------------
    // score::Series<double>
    // ----------------------------------------------------------------------
    nb::class_<score::Series<double>>(m, "Series")
        .def(nb::init<>(), "Construct an empty series.")
        .def(nb::init<std::vector<double>>(), "data"_a, "Construct from a sequence of doubles.")
        .def(nb::init<std::vector<double>, std::vector<std::string>>(), "data"_a, "dates"_a,
             "Construct from values and matching ISO date strings.")
        .def("__len__", &score::Series<double>::size)
        .def("__getitem__",
             [](const score::Series<double>& s, std::size_t idx) {
                 if (idx >= s.size()) { throw nb::index_error(); }
                 return s[idx];
             })
        .def(
            "__iter__",
            [](const score::Series<double>& s) {
                return nb::make_iterator(nb::type<score::Series<double>>(), "iterator", s.begin(), s.end());
            },
            nb::keep_alive<0, 1>())
        .def("size", &score::Series<double>::size)
        .def("empty", &score::Series<double>::empty)
        .def("values", &score::Series<double>::values, nb::rv_policy::reference_internal)
        .def("dates", &score::Series<double>::dates, nb::rv_policy::reference_internal)
        .def("__repr__",
             [](const score::Series<double>& s) { return "<score.Series size=" + std::to_string(s.size()) + ">"; });

    // ----------------------------------------------------------------------
    // score::statistics::DescriptiveStats
    // ----------------------------------------------------------------------
    nb::class_<score::statistics::DescriptiveStats>(m, "DescriptiveStats")
        .def(nb::init<const score::Series<double>&>(), "series"_a, nb::keep_alive<1, 2>(),
             "Construct from a Series.  The Series must outlive this object.")
        .def("mean", &score::statistics::DescriptiveStats::mean)
        .def("variance", &score::statistics::DescriptiveStats::variance, "sample"_a = true)
        .def("stddev", &score::statistics::DescriptiveStats::stddev, "sample"_a = true)
        .def("median", &score::statistics::DescriptiveStats::median)
        .def("min", &score::statistics::DescriptiveStats::min)
        .def("max", &score::statistics::DescriptiveStats::max)
        .def("range", &score::statistics::DescriptiveStats::range)
        .def("quantile", &score::statistics::DescriptiveStats::quantile, "q"_a)
        .def("skewness", &score::statistics::DescriptiveStats::skewness)
        .def("kurtosis", &score::statistics::DescriptiveStats::kurtosis);

    // ----------------------------------------------------------------------
    // score::metrics::StockAnalyzer
    // ----------------------------------------------------------------------
    nb::class_<score::metrics::StockAnalyzer>(m, "StockAnalyzer")
        .def(nb::init<const score::Series<double>&>(), "prices"_a, nb::keep_alive<1, 2>(),
             "Construct from a price Series.  The Series must outlive this object.")
        .def("simple_returns", &score::metrics::StockAnalyzer::simple_returns)
        .def("log_returns", &score::metrics::StockAnalyzer::log_returns)
        .def("cumulative_return", &score::metrics::StockAnalyzer::cumulative_return)
        .def("annualized_return", &score::metrics::StockAnalyzer::annualized_return, "periods_per_year"_a = 252)
        .def("annualized_volatility", &score::metrics::StockAnalyzer::annualized_volatility, "periods_per_year"_a = 252)
        .def("sharpe_ratio", &score::metrics::StockAnalyzer::sharpe_ratio, "risk_free_rate"_a = 0.0,
             "periods_per_year"_a = 252)
        .def("max_drawdown", &score::metrics::StockAnalyzer::max_drawdown);

    // ----------------------------------------------------------------------
    // score::statistics::cdf (normal distribution CDF)
    // ----------------------------------------------------------------------
    m.def("normal_cdf", &my_math::cdf, "x"_a, "mean"_a = 0.0, "stddev"_a = 1.0,
          "Standard/normal CDF via a rational erfc approximation.");

    m.def(
        "normal_cdf_batch",
        [](nb::ndarray<const double, nb::ndim<1>, nb::c_contig> x, double mean, double stddev) {
            const size_t n   = x.shape(0);
            auto*        out = new double[n];
            for (size_t i = 0; i < n; ++i) { out[i] = my_math::cdf(x(i), mean, stddev); }
            nb::capsule owner(out, [](void* p) noexcept { delete[] static_cast<double*>(p); });
            return nb::ndarray<nb::numpy, double, nb::ndim<1>>(out, {n}, owner);
        },
        "x"_a, "mean"_a = 0.0, "stddev"_a = 1.0, "Vectorized normal_cdf over a 1-D array of x values.");

    // ----------------------------------------------------------------------
    // risk::RiskStats & risk functions
    // ----------------------------------------------------------------------
    nb::class_<risk::RiskStats>(m, "RiskStats")
        .def_ro("mean_daily_return", &risk::RiskStats::mean_daily_return)
        .def_ro("daily_std_return", &risk::RiskStats::daily_std_return)
        .def_ro("n", &risk::RiskStats::n)
        .def("__repr__", [](const risk::RiskStats& s) {
            return "<RiskStats n=" + std::to_string(s.n) + " mean=" + std::to_string(s.mean_daily_return) +
                   " std=" + std::to_string(s.daily_std_return) + ">";
        });

    m.def("calculate_risk", &risk::calculate_risk, "prices"_a,
          "Calculate risk statistics (mean, daily std, n) from a price sequence.");

    m.def("calculate_risk_from_returns",
          [](const std::vector<double>& returns) { return risk::calculate_risk_from_returns(returns); }, "returns"_a,
          "Calculate risk statistics from a return sequence.");

    m.def("calculate_var", &risk::calculate_var, "stats"_a, "confidence"_a, "horizon_days"_a = 1,
          "Calculate parametric Value at Risk (VaR) for a given confidence level.");

    m.def("calculate_cvar", &risk::calculate_cvar, "stats"_a, "confidence"_a, "horizon_days"_a = 1,
          "Calculate parametric Conditional Value at Risk (CVaR) for a given confidence level.");

    m.def("log_returns", [](const std::vector<double>& prices) { return risk::log_returns(prices); }, "prices"_a,
          "Compute log returns from a price sequence.");

    // ----------------------------------------------------------------------
    // score::db — daily-bar column store
    // ----------------------------------------------------------------------
    nb::class_<score::db::Bar>(m, "Bar")
        .def(
            "__init__",
            [](score::db::Bar* self, std::string date, double open, double high, double low, double close,
               std::int64_t volume) { new (self) score::db::Bar{std::move(date), open, high, low, close, volume}; },
            "date"_a, "open"_a, "high"_a, "low"_a, "close"_a, "volume"_a, "One daily OHLCV bar; `date` is ISO-8601.")
        .def_rw("date", &score::db::Bar::date)
        .def_rw("open", &score::db::Bar::open)
        .def_rw("high", &score::db::Bar::high)
        .def_rw("low", &score::db::Bar::low)
        .def_rw("close", &score::db::Bar::close)
        .def_rw("volume", &score::db::Bar::volume)
        .def("__repr__", [](const score::db::Bar& b) {
            return "<Bar " + b.date + " o=" + std::to_string(b.open) + " h=" + std::to_string(b.high) +
                   " l=" + std::to_string(b.low) + " c=" + std::to_string(b.close) + " v=" + std::to_string(b.volume) +
                   ">";
        });

    nb::class_<score::db::ColumnStoreV1>(m, "ColumnStoreV1")
        .def(nb::init<std::filesystem::path>(), "root_dir"_a, "Open (creating if needed) the store rooted at root_dir.")
        .def("append", &score::db::ColumnStoreV1::append, "symbol"_a, "bars"_a,
             "Append bars for a symbol; dates must be strictly after everything already stored.")
        .def("scan", &score::db::ColumnStoreV1::scan, "symbol"_a, "start_date"_a, "end_date"_a,
             "Bars with date in [start_date, end_date].")
        .def("symbols", &score::db::ColumnStoreV1::symbols, "Every stored symbol, sorted.")
        .def("count", &score::db::ColumnStoreV1::count, "symbol"_a, "Number of bars stored for a symbol.");
    // TODO(E2): bind scan_column so it returns a read-only NumPy array that
    // shares the ColumnView's memory instead of copying it. See
    // tasks/code_snippets.md, "Phase E — handing mapped memory to NumPy".

    // ----------------------------------------------------------------------
    // Module metadata
    // ----------------------------------------------------------------------
    m.attr("__version__") = "0.1.0";
}
