#include "mc/sequential.h"
#include "mc/benchmark_samples.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace mc::seq {
namespace {
using Clock = std::chrono::steady_clock;

double elapsed(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

template<class Sample>
BenchmarkResult measure(const Config& cfg, Sample sample) {
    BenchmarkResult result;
    result.samples = cfg.samples;
    result.workers = 1;
    const auto start = Clock::now();
    for (std::uint64_t index = 0; index < cfg.samples; ++index)
        result.checksum += sample(index);
    result.t_total = elapsed(start);
    return result;
}
}

Result integrate(const Config& cfg) {
    validate(cfg);
    if (cfg.mode != Mode::Full)
        throw std::invalid_argument("integrate wymaga trybu Full");
    const unsigned strata = cfg.strata ? cfg.strata : 1;
    const auto budget = cfg.target_abs ? cfg.max_samples : cfg.samples;
    StrataStats totals{};
    Result result;
    result.workers = 1;
    const auto start = Clock::now();
    while (result.samples < budget && !result.reached) {
        const auto begin = result.samples;
        const auto count = std::min(cfg.batch, budget - begin);
        StrataStats local{};
        for (std::uint64_t offset = 0; offset < count; ++offset) {
            const auto index = begin + offset;
            local[index % strata].add(sample(cfg.function, cfg.seed, index, cfg.strata));
        }
        const auto reduce_start = Clock::now();
        for (unsigned k = 0; k < strata; ++k) totals[k].merge(local[k]);
        update_estimate(cfg, totals, result);
        result.t_reduce += elapsed(reduce_start);
    }
    result.t_total = elapsed(start);
    return result;
}

BenchmarkResult benchmark(const Config& cfg) {
    validate(cfg);
    if (cfg.mode == Mode::Full)
        throw std::invalid_argument("benchmark wymaga trybu RngOnly lub FuncOnly");
    const auto dim = dimensions(cfg.function);
    if (cfg.mode == Mode::RngOnly)
        return measure(cfg, RngSamples{cfg.seed, dim});
    return measure(cfg, FunctionSamples{cfg.function, dim});
}
} // namespace mc::seq
