/*
 * tests/cpp/test_guards.cpp
 *
 * Degenerate-input behavior for the descriptive/metric primitives, and
 * the moment conventions the risk layer depends on.
 *
 * These paths matter once the primitives are driven over thousands of
 * rolling windows: a single-observation window, a flat window, or a
 * zero-volatility window will occur, and each must fail loudly rather
 * than return NaN/inf that propagates silently downstream.
 */

#include <score/core/exceptions.hpp>
#include <score/core/types.hpp>
#include <score/metrics/stock_metrics.hpp>
#include <score/statistics/descriptive.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using score::Series;
using score::metrics::StockAnalyzer;
using score::statistics::DescriptiveStats;

TEST_CASE("DescriptiveStats::variance sample with n = 1 throws", "[stats][variance][guard]") {
    Series<double> s({42.0});
    DescriptiveStats d(s);
    REQUIRE_THROWS_AS(d.variance(true), score::DomainError);
    // The population variance of a single point is well defined (zero).
    REQUIRE_THAT(d.variance(false), WithinAbs(0.0, 1e-15));
}

TEST_CASE("DescriptiveStats::skewness uses the population convention", "[stats][skewness]") {
    // For {1,2,3,4,5}: mean 3, population variance 2, so sigma = sqrt(2).
    // Sum of cubed deviations is 0 by symmetry, hence g1 = 0 exactly.
    Series<double> s({1.0, 2.0, 3.0, 4.0, 5.0});
    REQUIRE_THAT(DescriptiveStats(s).skewness(), WithinAbs(0.0, 1e-15));
}

TEST_CASE("DescriptiveStats::kurtosis uses the population convention", "[stats][kurtosis]") {
    // For {1,2,3,4,5}: Sum d^4 = 16+1+0+1+16 = 34, sigma^2 = 2, so
    // g2 = 34 / (5 * 4) - 3 = 1.7 - 3 = -1.3.  (Mixing in the SAMPLE
    // stddev instead would give -3 + 34/(5*6.25) = -1.912, which is the
    // bug this test pins down.)
    Series<double> s({1.0, 2.0, 3.0, 4.0, 5.0});
    REQUIRE_THAT(DescriptiveStats(s).kurtosis(), WithinAbs(-1.3, 1e-12));
}

TEST_CASE("DescriptiveStats moments reject a zero-variance series", "[stats][guard]") {
    Series<double> flat({2.0, 2.0, 2.0, 2.0});
    DescriptiveStats d(flat);
    REQUIRE_THROWS_AS(d.skewness(), score::DomainError);
    REQUIRE_THROWS_AS(d.kurtosis(), score::DomainError);
}

TEST_CASE("StockAnalyzer::annualized_return needs two prices", "[metrics][guard]") {
    Series<double> one({100.0});
    REQUIRE_THROWS_AS(StockAnalyzer(one).annualized_return(), score::DomainError);
}

TEST_CASE("StockAnalyzer::sharpe_ratio rejects zero volatility", "[metrics][sharpe][guard]") {
    // A perfectly flat price path has zero return variance, so the ratio
    // is 0/0 rather than "infinitely good".
    Series<double> flat({100.0, 100.0, 100.0, 100.0});
    REQUIRE_THROWS_AS(StockAnalyzer(flat).sharpe_ratio(), score::DomainError);
}
