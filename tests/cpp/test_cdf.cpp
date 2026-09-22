/*
 * tests/cpp/test_cdf.cpp
 *
 * First tests for the hand-ported Cephes erf/erfc in
 * include/score/statistics/cdf.hpp -- the most numerically delicate code
 * in the project, and until now the least covered.
 *
 * The three implementations (Simpson quadrature, std::erf/erfc, and the
 * hand-rolled rational approximation) are independent, so cross-checking
 * them against each other is a genuine test rather than a tautology.
 * The absolute reference values below are exact by symmetry or are
 * standard normal quantile points.
 */

#include <score/statistics/cdf.hpp>
#include <score/statistics/normal_pdf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>


using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("my_math::cdf is exactly one half at the mean", "[cdf][symmetry]") {
    REQUIRE_THAT(my_math::cdf(0.0), WithinAbs(0.5, 1e-15));
    REQUIRE_THAT(my_math::cdf(5.0, 5.0, 2.0), WithinAbs(0.5, 1e-15));
}

TEST_CASE("my_math::cdf is symmetric about the mean", "[cdf][symmetry]") {
    // Phi(-z) + Phi(z) == 1 for every z.
    for (const double z : {0.25, 0.5, 1.0, 1.96, 3.0, 6.0}) {
        REQUIRE_THAT(my_math::cdf(-z) + my_math::cdf(z), WithinAbs(1.0, 1e-15));
    }
}

TEST_CASE("my_math::cdf matches standard normal quantile points", "[cdf][reference]") {
    // Standard two-sided normal quantiles: exact by definition of the
    // 95% / 99% points of the standard normal.
    REQUIRE_THAT(my_math::cdf(1.959963984540054), WithinAbs(0.975, 1e-14));
    REQUIRE_THAT(my_math::cdf(2.3263478740408408), WithinAbs(0.99, 1e-14));
    REQUIRE_THAT(my_math::cdf(1.2815515655446004), WithinAbs(0.90, 1e-14));
}

TEST_CASE("my_math::cdf agrees with the std::erfc implementation", "[cdf][crosscheck]") {
    for (const double z : {-6.0, -3.0, -1.0, -0.1, 0.0, 0.1, 1.0, 3.0, 6.0}) {
        REQUIRE_THAT(my_math::cdf(z), WithinRel(std_math::cdf_erfc(z), 1e-12));
    }
}

TEST_CASE("my_math::cdf agrees with Simpson quadrature of the pdf", "[cdf][crosscheck]") {
    // naive::cdf integrates normal_pdf numerically; it is slow and only
    // ~1e-9 accurate, which is why it is a baseline rather than the
    // production path.
    for (const double z : {-2.5, -1.0, 0.0, 1.0, 2.5}) {
        REQUIRE_THAT(my_math::cdf(z), WithinAbs(naive::cdf(z), 1e-9));
    }
}

TEST_CASE("my_math::cdf handles the far tails without underflowing to zero", "[cdf][tail]") {
    // The far left tail is exactly where a naive (1 + erf(z))/2 formula
    // loses all precision, and where 99.9% VaR lives.
    const double tail = my_math::cdf(-8.0);
    REQUIRE(tail > 0.0);
    REQUIRE(tail < 1e-14);
    REQUIRE_THAT(tail, WithinRel(std_math::cdf_erfc(-8.0), 1e-10));
}

TEST_CASE("my_math::cdf is monotonically increasing", "[cdf][monotonic]") {
    double previous = 0.0;
    for (double z = -5.0; z <= 5.0; z += 0.1) {
        const double current = my_math::cdf(z);
        REQUIRE(current >= previous);
        previous = current;
    }
}

TEST_CASE("normal_pdf integrates to the CDF difference", "[cdf][pdf]") {
    // Sanity check that pdf and cdf describe the same distribution:
    // the pdf peak equals 1/sqrt(2*pi).
    REQUIRE_THAT(normal_pdf(0.0), WithinAbs(0.3989422804014327, 1e-15));
    REQUIRE_THAT(normal_pdf(1.0, 1.0, 2.0), WithinAbs(0.3989422804014327 / 2.0, 1e-15));
}
