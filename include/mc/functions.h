#pragma once

#include "mc/philox.h"
#include <cmath>

namespace mc {
enum class Function { Sin, Gauss1d, Peak, GaussNd };
constexpr double pi = 3.14159265358979323846;

inline const char* name(Function function) {
    switch (function) {
        case Function::Sin: return "sin";
        case Function::Gauss1d: return "gauss1d";
        case Function::Peak: return "peak";
        case Function::GaussNd: return "gaussNd";
    }
    return "unknown";
}

inline unsigned dimensions(Function function) {
    return function == Function::GaussNd ? 4 : 1;
}

inline double exact(Function function) {
    const double gaussian = std::sqrt(pi) * 0.5 * std::erf(1.0);
    switch (function) {
        case Function::Sin: return 2;
        case Function::Gauss1d: return gaussian;
        case Function::Peak: return std::atan(5.0) / 5;
        case Function::GaussNd: return std::pow(gaussian, 4);
    }
    return 0;
}

// u contains normalized coordinates. Include the domain volume in each sample.
inline double evaluate(Function function, const double* u) {
    switch (function) {
        case Function::Sin: return pi * std::sin(pi * u[0]);
        case Function::Gauss1d: return std::exp(-u[0] * u[0]);
        case Function::Peak: return 1 / (1 + 25 * u[0] * u[0]);
        case Function::GaussNd: {
            double radius = 0;
            for (unsigned d = 0; d < 4; ++d) radius += u[d] * u[d];
            return std::exp(-radius);
        }
    }
    return 0;
}

inline double sample(Function function, std::uint64_t seed, std::uint64_t index,
                     unsigned strata = 0) {
    double u[4]{};
    for (unsigned d = 0; d < dimensions(function); ++d)
        u[d] = uniform(seed, index, d);
    if (strata != 0)
        u[0] = (static_cast<double>(index % strata) + u[0]) / strata;
    return evaluate(function, u);
}
} // namespace mc
