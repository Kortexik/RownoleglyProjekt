#pragma once

#include "mc/parameters.h"
#include "mc/result.h"
#include "mc/stats.h"
#include <array>

namespace mc {
inline constexpr unsigned stratum_count = 64;
using StrataStats = std::array<Stats, stratum_count>;

void validate(const Config& config);
// Updates the estimate, sample count and stopping decision; preserves timings.
void update_estimate(const Config& config, const StrataStats& totals, Result& result);
} // namespace mc
