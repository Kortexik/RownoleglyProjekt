#include "mc/openmp.h"

#include <omp.h>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace mc {
Result integrate(const Config& cfg) {
    validate(cfg);
    if (cfg.mode != Mode::Full)
        throw std::invalid_argument("integrate wymaga trybu Full");
    const int requested = cfg.threads ? cfg.threads : omp_get_max_threads();
    const unsigned strata = cfg.strata ? cfg.strata : 1;
    const std::uint64_t budget = cfg.target_abs ? cfg.max_samples : cfg.samples;
    std::vector<StrataStats> partial(static_cast<std::size_t>(requested));
    StrataStats totals{};
    Result result;
    bool running = true;
    std::uint64_t begin = 0, count = 0;
    const double start = omp_get_wtime();

    #pragma omp parallel num_threads(requested) shared(result, running, begin, count, partial, totals)
    {
        const int worker = omp_get_thread_num();
        #pragma omp single
        result.workers = omp_get_num_threads();

        while (running) {
            StrataStats local{};
            #pragma omp single
            {
                begin = result.samples;
                count = std::min(cfg.batch, budget - begin);
            }
            #pragma omp for schedule(static) nowait
            for (std::uint64_t offset = 0; offset < count; ++offset) {
                const auto index = begin + offset;
                local[index % strata].add(sample(cfg.function, cfg.seed, index, cfg.strata));
            }
            partial[worker] = local;
            #pragma omp barrier
            #pragma omp single
            {
                const double reduce_start = omp_get_wtime();
                for (int t = 0; t < result.workers; ++t)
                    for (unsigned k = 0; k < strata; ++k)
                        totals[k].merge(partial[t][k]);
                update_estimate(cfg, totals, result);
                running = result.samples < budget && !result.reached;
                result.t_reduce += omp_get_wtime() - reduce_start;
            }
        }
    }
    result.t_total = omp_get_wtime() - start;
    return result;
}
} // namespace mc
