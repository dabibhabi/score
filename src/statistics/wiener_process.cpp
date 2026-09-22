#include <score/core/exceptions.hpp>
#include <score/statistics/wiener_process.hpp>

#include <cmath>
#include <random>

namespace wiener {
namespace {

/// Resolve the caller's optional seed into a concrete one.
///
/// Factored out so every future stochastic routine seeds the same way,
/// and so a caller can see exactly where non-determinism enters.
unsigned int resolve_seed(std::optional<unsigned int> seed) {
    if (seed.has_value()) {
        return seed.value(); // explicit seed -> reproducible
    }
    std::random_device rd;
    return rd();
}

/// Number of increments on a uniform grid of `time_step` covering
/// `total_time`.
///
/// Computed once from the ratio rather than by accumulating `t += dt` in
/// the loop: accumulation drifts by a few ULPs, and the comparison
/// `t + dt <= total_time` then silently drops the final step.  With
/// total_time = 1.0 and time_step = 0.01 that produced a 100-point path
/// where 101 is correct.  The epsilon absorbs the rounding in the ratio
/// itself for grids that divide evenly.
std::size_t step_count(double total_time, double time_step) {
    const double ratio = total_time / time_step;
    return static_cast<std::size_t>(std::floor(ratio + 1e-9));
}

} // namespace

std::vector<double> wiener_process(double total_time, double time_step,
                                   std::optional<unsigned int> seed) {
    if (time_step <= 0.0) {
        throw score::DomainError("wiener_process: time_step must be positive");
    }
    if (total_time < 0.0) {
        throw score::DomainError("wiener_process: total_time must be non-negative");
    }

    std::mt19937 gen(resolve_seed(seed));
    std::normal_distribution<double> Z; // N(0, 1)

    const std::size_t n_steps = step_count(total_time, time_step);
    const double sqrt_dt = std::sqrt(time_step);

    std::vector<double> W;
    W.reserve(n_steps + 1);
    W.push_back(0.0); // W(0) = 0 by definition

    for (std::size_t k = 0; k < n_steps; ++k) {
        // Increments are i.i.d. N(0, dt), so scaling a standard normal by
        // sqrt(dt) gives W(t + dt) - W(t).
        W.push_back(W.back() + Z(gen) * sqrt_dt);
    }

    return W;
}

} // namespace wiener
