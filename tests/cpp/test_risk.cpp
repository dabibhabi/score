// historical_var / historical_cvar. This code is already implemented, so
// these tests are enabled and must pass.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <score/core/exceptions.hpp>
#include <score/statistics/risk.hpp>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

// -0.05, -0.04, ..., 0.04: ten returns, one per percent.
std::vector<double> ten_returns() { return {-0.05, -0.04, -0.03, -0.02, -0.01, 0.0, 0.01, 0.02, 0.03, 0.04}; }

// -0.50, -0.49, ..., 0.49: one hundred returns.
std::vector<double> hundred_returns() {
    std::vector<double> r;
    for (int i = 0; i < 100; ++i) { r.push_back(i / 100.0 - 0.5); }
    return r;
}

}  // namespace

TEST_CASE("historical_var: tail of one and two observations out of ten", "[risk][var]") {
    // tail size = floor((1 - c) * n); VaR is the least extreme loss inside the tail.
    REQUIRE_THAT(risk::historical_var(ten_returns(), 0.90), WithinAbs(0.05, 1e-12));  // tail = {-0.05}
    REQUIRE_THAT(risk::historical_var(ten_returns(), 0.80), WithinAbs(0.04, 1e-12));  // tail = {-0.05, -0.04}
}

TEST_CASE("historical_cvar: mean loss inside the tail", "[risk][cvar]") {
    REQUIRE_THAT(risk::historical_cvar(ten_returns(), 0.90), WithinAbs(0.05, 1e-12));
    REQUIRE_THAT(risk::historical_cvar(ten_returns(), 0.80), WithinAbs(0.045, 1e-12));  // (0.05 + 0.04) / 2
}

TEST_CASE("historical VaR/CVaR: 95% on one hundred returns", "[risk][var][cvar]") {
    // tail = the 5 worst: -0.50 .. -0.46
    REQUIRE_THAT(risk::historical_var(hundred_returns(), 0.95), WithinAbs(0.46, 1e-12));
    REQUIRE_THAT(risk::historical_cvar(hundred_returns(), 0.95), WithinAbs(0.48, 1e-12));
}

TEST_CASE("historical VaR/CVaR: default confidence level is 95%", "[risk][var][cvar]") {
    REQUIRE(risk::historical_var(hundred_returns()) == risk::historical_var(hundred_returns(), 0.95));
    REQUIRE(risk::historical_cvar(hundred_returns()) == risk::historical_cvar(hundred_returns(), 0.95));
}

TEST_CASE("historical VaR/CVaR: input order does not matter", "[risk][var][cvar]") {
    auto shuffled = hundred_returns();
    std::reverse(shuffled.begin(), shuffled.end());
    std::rotate(shuffled.begin(), shuffled.begin() + 37, shuffled.end());
    REQUIRE(risk::historical_var(shuffled, 0.95) == risk::historical_var(hundred_returns(), 0.95));
    REQUIRE(risk::historical_cvar(shuffled, 0.95) == risk::historical_cvar(hundred_returns(), 0.95));
}

TEST_CASE("historical VaR/CVaR: CVaR is never smaller than VaR", "[risk][var][cvar]") {
    for (double c : {0.80, 0.90, 0.95, 0.99}) {
        INFO("confidence " << c);
        REQUIRE(risk::historical_cvar(hundred_returns(), c) >= risk::historical_var(hundred_returns(), c));
    }
}

TEST_CASE("historical VaR/CVaR: a series with no losses has zero risk", "[risk][var][cvar]") {
    const std::vector<double> gains = {0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09, 0.10};
    REQUIRE(risk::historical_var(gains, 0.90) == 0.0);
    REQUIRE(risk::historical_cvar(gains, 0.90) == 0.0);
}

TEST_CASE("historical VaR/CVaR: invalid input", "[risk][var][cvar]") {
    REQUIRE_THROWS_AS(risk::historical_var({}, 0.95), score::EmptySeriesError);
    REQUIRE_THROWS_AS(risk::historical_cvar({}, 0.95), score::EmptySeriesError);

    for (double c : {0.0, 1.0, -0.1, 1.5}) {
        INFO("confidence " << c);
        REQUIRE_THROWS_AS(risk::historical_var(ten_returns(), c), score::DomainError);
        REQUIRE_THROWS_AS(risk::historical_cvar(ten_returns(), c), score::DomainError);
    }
}

TEST_CASE("historical VaR/CVaR: too few returns for the requested quantile", "[risk][var][cvar]") {
    // floor(0.05 * 10) = 0 observations in the tail: there is nothing to report.
    REQUIRE_THROWS_AS(risk::historical_var(ten_returns(), 0.95), score::DomainError);
    REQUIRE_THROWS_AS(risk::historical_cvar(ten_returns(), 0.95), score::DomainError);
}
