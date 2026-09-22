#pragma once

#include <score/core/types.hpp>

namespace score::statistics {

/// Descriptive statistics over a Series<double>.
///
/// The class is a thin view: it stores a const reference to the input
/// series and computes statistics on demand.  The lifetime of the input
/// must outlive the DescriptiveStats instance.
class DescriptiveStats {
  public:
    /// Construct from a series.  Statistics that require non-empty input
    /// will throw EmptySeriesError lazily, when called.
    explicit DescriptiveStats(const Series<double>& series);

    /// Arithmetic mean.
    ///   μ = (1/n) Σ x_i
    [[nodiscard]] double mean() const;

    /// Variance.  When `sample` is true, returns the unbiased sample
    /// variance s² = (1/(n-1)) Σ (x_i - μ)²; otherwise the population
    /// variance σ² = (1/n) Σ (x_i - μ)².
    ///
    /// Throws DomainError when `sample` is true and n < 2, rather than
    /// dividing by zero.
    [[nodiscard]] double variance(bool sample = true) const;

    /// Standard deviation, sqrt of variance().
    [[nodiscard]] double stddev(bool sample = true) const;

    /// Median (50th percentile).
    [[nodiscard]] double median() const;

    /// Smallest element of the series.
    [[nodiscard]] double min() const;

    /// Largest element of the series.
    [[nodiscard]] double max() const;

    /// max() - min().
    [[nodiscard]] double range() const;

    /// q-quantile via linear interpolation between order statistics.
    /// Throws DomainError if q is outside [0, 1].
    [[nodiscard]] double quantile(double q) const;

    /// Skewness, population convention:
    ///   g_1 = (1/n) Σ ((x_i - μ)/σ)³,   σ = population stddev
    ///
    /// Both the moment sum and the normalizing stddev use the n
    /// denominator.  This is the convention Cornish-Fisher expansions in
    /// score::risk assume; mixing sample and population denominators here
    /// would bias those quantiles by ((n-1)/n)^(3/2).
    ///
    /// Throws DomainError on a zero-variance series, where it is undefined.
    [[nodiscard]] double skewness() const;

    /// Excess kurtosis, population convention:
    ///   g_2 = (1/n) Σ ((x_i - μ)/σ)⁴ - 3,   σ = population stddev
    ///
    /// Throws DomainError on a zero-variance series, where it is undefined.
    [[nodiscard]] double kurtosis() const;

  private:
    const Series<double>& series_;
};

} // namespace score::statistics
