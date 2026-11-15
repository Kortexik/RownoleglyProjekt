#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace mc {
struct Stats {
    std::uint64_t n = 0;
    double mean = 0;
    double m2 = 0;

    void add(double value) {
        ++n;
        const double delta = value - mean;
        mean += delta / static_cast<double>(n);
        m2 += delta * (value - mean);
    }

    void merge(const Stats& other) {
        if (other.n == 0) return;
        if (n == 0) { *this = other; return; }
        const double total = static_cast<double>(n + other.n);
        const double delta = other.mean - mean;
        m2 += other.m2 + delta * delta * static_cast<double>(n)
                                      * static_cast<double>(other.n) / total;
        mean += delta * static_cast<double>(other.n) / total;
        n += other.n;
    }

    double variance() const {
        return n > 1 ? std::max(0.0, m2 / static_cast<double>(n - 1))
                     : std::numeric_limits<double>::infinity();
    }
};
} // namespace mc
