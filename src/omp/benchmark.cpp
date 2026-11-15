#include "mc/openmp.h"
#include "mc/benchmark_samples.h"

#include <omp.h>
#include <stdexcept>
#include <vector>

namespace mc {
namespace {
template<class Sample>
BenchmarkResult measure(const Config& cfg, Sample sample) {
    const int requested = cfg.threads ? cfg.threads : omp_get_max_threads();
    std::vector<double> partial(static_cast<std::size_t>(requested));
    BenchmarkResult result;
    result.samples = cfg.samples;
    const double start = omp_get_wtime();
    #pragma omp parallel num_threads(requested)
    {
        const int worker = omp_get_thread_num();
        double checksum = 0;
        #pragma omp single nowait
        result.workers = omp_get_num_threads();
        #pragma omp for schedule(static) nowait
        for (std::uint64_t index = 0; index < cfg.samples; ++index)
            checksum += sample(index);
        partial[worker] = checksum;
    }
    const double reduce_start = omp_get_wtime();
    for (int worker = 0; worker < result.workers; ++worker)
        result.checksum += partial[worker];
    result.t_reduce = omp_get_wtime() - reduce_start;
    result.t_total = omp_get_wtime() - start;
    return result;
}
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
} // namespace mc
