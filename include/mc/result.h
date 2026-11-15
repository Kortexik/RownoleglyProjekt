#pragma once

#include <cstdint>

namespace mc {
struct Result {
    std::uint64_t samples = 0;
    int workers = 0;
    double estimate = 0;
    double std_err = 0;
    double ci_lo = 0;
    double ci_hi = 0;
    bool reached = false;
    double t_total = 0;
    double t_reduce = 0;
};

struct BenchmarkResult {
    std::uint64_t samples = 0;
    int workers = 0;
    double checksum = 0;
    double t_total = 0;
    double t_reduce = 0;
};
} // namespace mc
