#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <score/core/exceptions.hpp>
#include <score/statistics/cdf.hpp>
#include <score/statistics/normal_pdf.hpp>
#include <score/statistics/risk.hpp>
#include <string>
#include <utility>
#include <vector>

namespace risk {

namespace {

// Shared validation + sort for the historical-simulation VaR/CVaR estimators:
// both need the returns sorted ascending and the same tail-size check. The tail
// is sorted_returns[0, tail_index): its last element (tail_index - 1) is the VaR
// quantile and its mean is CVaR.
struct HistoricalTail {
    std::vector<double> sorted_returns;
    std::size_t          tail_index;
};

HistoricalTail historical_tail(const std::vector<double>& returns, double confidence_level, const char* caller) {
    if (returns.empty()) { throw score::EmptySeriesError(std::string(caller) + ": returns series is empty"); }

    if (confidence_level <= 0.0 || confidence_level >= 1.0) {
        throw score::DomainError(std::string(caller) +
                                  ": confidence_level must be in (0, 1), got " + std::to_string(confidence_level));
    }

    // A tiny epsilon absorbs binary floating-point error in (1 - confidence_level)
    // (e.g. 1.0 - 0.90 == 0.09999999999999998), which would otherwise truncate
    // an exact boundary like n=10, confidence_level=0.90 down to tail_index 0.
    constexpr double  kEpsilon     = 1e-9;
    const double      tail_fraction = 1.0 - confidence_level;
    const std::size_t tail_index =
        static_cast<std::size_t>(tail_fraction * static_cast<double>(returns.size()) + kEpsilon);
    if (tail_index == 0) {
        throw score::DomainError(std::string(caller) + ": not enough returns (" + std::to_string(returns.size()) +
                                  ") to resolve the " + std::to_string(confidence_level) + " confidence quantile");
    }

    std::vector<double> sorted_returns = returns;
    std::sort(sorted_returns.begin(), sorted_returns.end());
    return HistoricalTail{std::move(sorted_returns), tail_index};
}

}  // namespace
std::vector<double> log_returns(const std::vector<double>& prices) {
    if (prices.empty()) { throw score::EmptySeriesError("risk::log_returns: prices series is empty"); }

    if (prices.size() < 2) {
        throw score::DomainError("risk::log_returns requires at least 2 prices, got " + std::to_string(prices.size()));
    }

    if (prices[0] <= 0.0) {
        throw score::DomainError("risk::log_returns: price at index 0 must be positive, got " +
                                 std::to_string(prices[0]));
    }

    std::vector<double> returns;
    returns.reserve(prices.size() - 1);
    for (std::size_t i = 1; i < prices.size(); i++) {
        if (prices[i] <= 0.0) {
            throw score::DomainError("risk::log_returns: price at index " + std::to_string(i) +
                                     " must be positive, got " + std::to_string(prices[i]));
        }
        returns.push_back(std::log(prices[i] / prices[i - 1]));
    }
    return returns;
}

RiskStats calculate_risk_from_returns(const std::vector<double>& returns) {
    if (returns.empty()) {
        throw score::EmptySeriesError("risk::calculate_risk_from_returns: returns series is empty");
    }

    if (returns.size() < 2) {
        throw score::DomainError("risk::calculate_risk requires at least 2 returns to compute sample variance, got " +
                                 std::to_string(returns.size()));
    }

    RiskStats stats{};
    stats.n = returns.size();

    const double sum        = std::accumulate(returns.begin(), returns.end(), 0.0);
    stats.mean_daily_return = sum / static_cast<double>(stats.n);

    double sum_sq = 0.0;
    for (double r : returns) {
        const double diff = r - stats.mean_daily_return;
        sum_sq += diff * diff;
    }
    stats.daily_std_return = std::sqrt(sum_sq / static_cast<double>(stats.n - 1));

    return stats;
}

RiskStats calculate_risk(const std::vector<double>& prices) { return calculate_risk_from_returns(log_returns(prices)); }

double historical_var(const std::vector<double>& returns, double confidence_level) {
    const HistoricalTail tail = historical_tail(returns, confidence_level, "risk::historical_var");
    return std::max(0.0, -tail.sorted_returns[tail.tail_index - 1]);
}

double calculate_var(const RiskStats& stats, double confidence, int horizon_days) {
    if (confidence <= 0.0 || confidence >= 1.0) {
        throw score::DomainError("risk::calculate_var: confidence must be strictly in (0, 1)");
    }
    if (horizon_days < 1) {
        throw score::DomainError("risk::calculate_var: horizon_days must be >= 1");
    }

    const double alpha = 1.0 - confidence;
    const double z = std_math::quantile(alpha);
    const double h = static_cast<double>(horizon_days);
    return -(stats.mean_daily_return * h + z * stats.daily_std_return * std::sqrt(h));
}

double calculate_cvar(const RiskStats& stats, double confidence, int horizon_days) {
    if (confidence <= 0.0 || confidence >= 1.0) {
        throw score::DomainError("risk::calculate_cvar: confidence must be strictly in (0, 1)");
    }
    if (horizon_days < 1) {
        throw score::DomainError("risk::calculate_cvar: horizon_days must be >= 1");
    }

    const double alpha = 1.0 - confidence;
    const double z = std_math::quantile(alpha);
    const double h = static_cast<double>(horizon_days);
    const double phi_z = normal_pdf(z);
    return -stats.mean_daily_return * h + stats.daily_std_return * std::sqrt(h) * (phi_z / alpha);
}

double historical_cvar(const std::vector<double>& returns, double confidence_level) {
    const HistoricalTail tail = historical_tail(returns, confidence_level, "risk::historical_cvar");
    const double tail_sum = std::accumulate(tail.sorted_returns.begin(), tail.sorted_returns.begin() + tail.tail_index, 0.0);
    return std::max(0.0, -(tail_sum / static_cast<double>(tail.tail_index)));
}
}  // namespace risk
