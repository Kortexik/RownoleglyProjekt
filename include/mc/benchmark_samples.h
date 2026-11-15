#pragma once

#include "mc/functions.h"
#include <cmath>

namespace mc {
struct RngSamples {
    std::uint64_t seed;
    unsigned dim;

    double operator()(std::uint64_t index) const {
        double sum = 0;
        for (unsigned d = 0; d < dim; ++d) sum += uniform(seed, index, d);
        return sum;
    }
};

struct FunctionSamples {
    Function function;
    unsigned dim;

    double operator()(std::uint64_t index) const {
        double u[4]{};
        for (unsigned d = 0; d < dim; ++d)
            u[d] = std::fmod((static_cast<double>(index % 1048576) + 0.5)
                             / 1048576 + d * 0.25, 1.0);
        return evaluate(function, u);
    }
};
} // namespace mc
