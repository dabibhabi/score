/*
 * tests/cpp/test_wiener.cpp
 *
 * First tests for wiener::wiener_process, which until now was not even
 * compiled into libscore_core.  Statistical assertions use a fixed seed
 * and generous tolerances: they check that the process is a Brownian
 * motion, not that a particular RNG stream is reproduced byte for byte.
 */

#include <score/core/types.hpp>
#include <score/statistics/descriptive.hpp>
#include <score/statistics/wiener_process.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <vector>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("wiener_process starts at zero with the expected length", "[wiener][shape]") {
    const double total_time = 1.0;
    const double dt = 0.01;
    const auto path = wiener::wiener_process(total_time, dt, 12345U);

    REQUIRE(path.size() == 101); // W(0) plus 100 steps
    REQUIRE_THAT(path.front(), WithinAbs(0.0, 1e-15));
}

TEST_CASE("wiener_process is reproducible for a fixed seed", "[wiener][seed]") {
    const auto a = wiener::wiener_process(1.0, 0.01, 7U);
    const auto b = wiener::wiener_process(1.0, 0.01, 7U);
    REQUIRE(a == b);

    const auto c = wiener::wiener_process(1.0, 0.01, 8U);
    REQUIRE(a != c);
}

TEST_CASE("wiener_process increments have variance dt", "[wiener][distribution]") {
    // Var(W(t + dt) - W(t)) = dt, and increments are independent, so the
    // sample variance over many steps converges to dt.
    const double dt = 0.01;
    const auto path = wiener::wiener_process(500.0, dt, 2024U);
    REQUIRE(path.size() > 40000);

    std::vector<double> increments;
    increments.reserve(path.size() - 1);
    for (std::size_t i = 1; i < path.size(); ++i) {
        increments.push_back(path[i] - path[i - 1]);
    }

    const score::Series<double> series(std::move(increments));
    const score::statistics::DescriptiveStats stats(series);

    REQUIRE_THAT(stats.mean(), WithinAbs(0.0, 5e-4));
    REQUIRE_THAT(stats.variance(true), WithinRel(dt, 0.05));
}

TEST_CASE("wiener_process terminal variance grows linearly in time", "[wiener][distribution]") {
    // Var(W(T)) = T.  Estimated across independent paths rather than
    // along one, since a single path gives only one terminal draw.
    const double total_time = 4.0;
    std::vector<double> terminals;
    terminals.reserve(2000);
    for (unsigned int seed = 0; seed < 2000U; ++seed) {
        terminals.push_back(wiener::wiener_process(total_time, 0.01, seed).back());
    }

    const score::Series<double> series(std::move(terminals));
    const score::statistics::DescriptiveStats stats(series);

    REQUIRE_THAT(stats.mean(), WithinAbs(0.0, 0.15));
    REQUIRE_THAT(stats.variance(true), WithinRel(total_time, 0.10));
}
