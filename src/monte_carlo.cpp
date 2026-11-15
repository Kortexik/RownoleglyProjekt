#include "mc/monte_carlo.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace mc {
namespace {
constexpr double confidence_z = 1.96;
constexpr std::uint64_t minimum_accuracy_samples = 4096;
}

void validate(const Config& cfg) {
    if (cfg.batch == 0) throw std::invalid_argument("batch musi byc dodatnie");
    if (cfg.threads < 0) throw std::invalid_argument("threads nie moze byc ujemne");
    if (cfg.strata != 0 && cfg.strata != stratum_count)
        throw std::invalid_argument("strata musi wynosic 0 lub 64");
    if (cfg.strata && dimensions(cfg.function) != 1)
        throw std::invalid_argument("warstwowanie wymaga funkcji jednowymiarowej");
    if (cfg.mode != Mode::Full && (cfg.target_abs || cfg.strata))
        throw std::invalid_argument("tryby pomiarowe nie obsluguja celu ani warstwowania");
    if (cfg.target_abs && (!std::isfinite(*cfg.target_abs) || *cfg.target_abs <= 0))
        throw std::invalid_argument("target_abs musi byc skonczone i dodatnie");
    const auto budget = cfg.target_abs ? cfg.max_samples : cfg.samples;
    const auto minimum = cfg.strata ? 2 * cfg.strata : 2;
    if (budget < minimum)
        throw std::invalid_argument("potrzeba co najmniej dwoch probek na warstwe");
}

void update_estimate(const Config& cfg, const StrataStats& totals, Result& result) {
    const unsigned count = cfg.strata ? cfg.strata : 1;
    result.samples = 0;
    result.estimate = 0;
    double variance = 0;
    for (unsigned k = 0; k < count; ++k) {
        result.samples += totals[k].n;
        result.estimate += totals[k].mean / count;
        variance += totals[k].n > 1
            ? totals[k].variance() / static_cast<double>(totals[k].n) / count / count
            : std::numeric_limits<double>::infinity();
    }
    result.std_err = std::sqrt(variance);
    const double half_width = confidence_z * result.std_err;
    result.ci_lo = result.estimate - half_width;
    result.ci_hi = result.estimate + half_width;
    result.reached = cfg.target_abs && result.samples >= minimum_accuracy_samples
        && half_width <= *cfg.target_abs;
}
} // namespace mc
