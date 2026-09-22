#pragma once

#include <optional>
#include <vector>

namespace wiener {

/// Standard Wiener process (Brownian motion) sampled on a uniform grid.
///
/// Returns W(0), W(dt), W(2·dt), ... up to the largest multiple of
/// `time_step` that does not exceed `total_time`.  W(0) is always 0, and
/// increments are i.i.d. N(0, time_step).
///
/// When `seed` is omitted the generator is seeded from std::random_device,
/// so results differ between runs; pass a seed for reproducibility.
///
/// Throws score::DomainError when `time_step` is not positive or
/// `total_time` is negative.
std::vector<double> wiener_process(double total_time, double time_step,
                                   std::optional<unsigned int> seed = std::nullopt);

} // namespace wiener
