#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace risk {

struct RiskStats {
    double      mean_daily_return;
    double      daily_std_return;
    std::size_t n;
};

std::vector<double> log_returns(std::span<const double> prices);

RiskStats calculate_risk(const std::vector<double>& prices);

RiskStats calculate_risk_from_returns(std::span<const double> returns);

double calculate_var(const RiskStats& stats, double confidence, int horizon_days = 1);

double calculate_cvar(const RiskStats& stats, double confidence, int horizon_days = 1);

/// Historical-simulation Value at Risk at `confidence_level` (e.g. 0.95),
/// returned as a positive loss fraction. Throws score::EmptySeriesError if
/// `returns` is empty, score::DomainError if confidence_level is outside
/// (0, 1) or there aren't enough points to resolve the requested quantile.
double historical_var(std::span<const double> returns, double confidence_level = 0.95);

/// Historical-simulation Conditional VaR (Expected Shortfall): the mean loss
/// in the tail beyond the historical_var threshold. Same exception
/// conventions as historical_var.
double historical_cvar(std::span<const double> returns, double confidence_level = 0.95);

}  // namespace risk
