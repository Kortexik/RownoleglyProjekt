#pragma once

#include "mc/functions.h"
#include <cstdint>
#include <optional>

namespace mc {
enum class Mode { Full, RngOnly, FuncOnly };

// Stable defaults for independently constructed runs and test cases.
struct Config {
    Function function = Function::Gauss1d;
    Mode mode = Mode::Full;
    std::uint64_t samples = 1'000'000;
    std::uint64_t seed = 42;
    std::uint64_t batch = 65'536;
    std::uint64_t max_samples = 100'000'000;
    std::optional<double> target_abs = std::nullopt;
    int threads = 4;
    unsigned strata = 0;
};
} // namespace mc
