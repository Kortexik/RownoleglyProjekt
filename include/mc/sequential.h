#pragma once

#include "mc/monte_carlo.h"

namespace mc::seq {
Result integrate(const Config& config);
BenchmarkResult benchmark(const Config& config);
} // namespace mc::seq
