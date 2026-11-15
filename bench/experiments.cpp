#include "mc/openmp.h"
#include "mc/sequential.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
struct Experiment {
    const char* group;
    int repeat;
    bool sequential;
    mc::Config config;
};

std::vector<Experiment> experiments() {
    std::vector<Experiment> cases;
    auto add = [&](const char* group, int repeat, bool seq, mc::Config config) {
        if (seq) config.threads = 1;
        cases.push_back({group, repeat, seq, config});
    };
    const mc::Function functions[] = {
        mc::Function::Sin, mc::Function::Gauss1d, mc::Function::Peak, mc::Function::GaussNd};

    for (auto function : functions)
    for (unsigned strata : {0U, 64U}) {
        if (strata && function == mc::Function::GaussNd) continue;
        mc::Config config;
        config.function = function;
        config.strata = strata;
        config.samples = 100003;
        config.batch = 7001;
        for (int threads : {1, 2, 4, 8, 14, 16}) {
            config.threads = threads;
            add("reproducibility", 1, false, config);
        }
        add("reproducibility", 1, true, config);

        for (auto samples : {10000ULL, 100000ULL, 1000000ULL})
        for (unsigned seed = 1; seed <= 100; ++seed) {
            config.samples = samples;
            config.batch = 65536;
            config.seed = seed;
            config.threads = 1;
            add("fixed_accuracy", seed, false, config);
            add("fixed_accuracy", seed, true, config);
        }

        for (double target : {0.01, 0.001, 0.0001})
        for (int repeat = 1; repeat <= 5; ++repeat) {
            config.target_abs = target;
            config.max_samples = 100000000;
            config.seed = 41 + repeat;
            config.batch = 65536;
            config.threads = 4;
            add("targets", repeat, false, config);
            add("targets", repeat, true, config);
        }
        config.target_abs = 1e-15;
        config.max_samples = 10003;
        config.batch = 1000;
        config.seed = 42;
        config.threads = 4;
        add("limits", 1, false, config);
        add("limits", 1, true, config);
    }

    for (auto function : {mc::Function::Gauss1d, mc::Function::GaussNd}) {
        for (auto samples : {10000ULL, 1000000ULL, 10000000ULL})
        for (int repeat = 1; repeat <= 7; ++repeat) {
            mc::Config config;
            config.function = function;
            config.samples = samples;
            for (int threads : {1, 2, 4, 8, 14, 16}) {
                config.threads = threads;
                add("scaling", repeat, false, config);
            }
            add("scaling", repeat, true, config);
        }
        for (int repeat = 1; repeat <= 7; ++repeat) {
            mc::Config config;
            config.function = function;
            config.samples = 10000000;
            config.batch = 1048576;
            for (int threads : {1, 2, 4, 8, 14, 16}) {
                config.threads = threads;
                add("scaling_large_batch", repeat, false, config);
            }
            add("scaling_large_batch", repeat, true, config);
        }
        for (auto mode : {mc::Mode::Full, mc::Mode::RngOnly, mc::Mode::FuncOnly})
        for (int repeat = 1; repeat <= 5; ++repeat) {
            mc::Config config;
            config.function = function;
            config.mode = mode;
            config.samples = 10000000;
            config.batch = 1048576;
            for (int threads : {1, 4, 8}) {
                config.threads = threads;
                add("cost_modes", repeat, false, config);
            }
            add("cost_modes", repeat, true, config);
        }
    }

    for (auto function : {mc::Function::Gauss1d, mc::Function::Peak})
    for (unsigned strata : {0U, 64U})
    for (unsigned seed = 1; seed <= 200; ++seed) {
        mc::Config config;
        config.function = function;
        config.strata = strata;
        config.target_abs = strata ? 0.00005 : 0.003;
        config.max_samples = 2000000;
        config.batch = 1024;
        config.threads = 1;
        config.seed = seed;
        add("stopped_coverage", seed, false, config);
        add("stopped_coverage", seed, true, config);
    }

    for (auto batch : {1024ULL, 4096ULL, 65536ULL, 1048576ULL})
    for (int repeat = 1; repeat <= 5; ++repeat)
    for (bool target : {false, true}) {
        mc::Config config;
        config.function = target ? mc::Function::Peak : mc::Function::Gauss1d;
        config.samples = 10000000;
        config.batch = batch;
        config.strata = target ? 64 : 0;
        if (target) config.target_abs = 0.001;
        const char* group = target ? "batch_target" : "batch_fixed";
        for (int threads : {4, 14}) {
            config.threads = threads;
            add(group, repeat, false, config);
        }
        add(group, repeat, true, config);
    }

    std::mt19937 random(20261115);
    std::shuffle(cases.begin(), cases.end(), random);
    return cases;
}

const char* mode_name(mc::Mode mode) {
    return mode == mc::Mode::Full ? "full" : mode == mc::Mode::RngOnly ? "rng-only" : "func-only";
}

void emit(const Experiment& experiment, std::size_t index) {
    const auto& config = experiment.config;
    const bool full = config.mode == mc::Mode::Full;
    mc::Result estimate;
    mc::BenchmarkResult benchmark;
    if (full) estimate = experiment.sequential
        ? mc::seq::integrate(config) : mc::integrate(config);
    else benchmark = experiment.sequential
        ? mc::seq::benchmark(config) : mc::benchmark(config);
    const auto samples = full ? estimate.samples : benchmark.samples;
    const int workers = full ? estimate.workers : benchmark.workers;
    const double total = full ? estimate.t_total : benchmark.t_total;
    const double reduce = full ? estimate.t_reduce : benchmark.t_reduce;
    const char* mode = mode_name(config.mode);
    std::cout << "case_id,group,repeat,backend,func,dim,method,mode,samples,seed,batch,"
                 "requested_workers,workers,max_samples,estimate,exact,abs_err,rel_err,"
                 "std_err,ci_lo,ci_hi,ci_hit,target,reached,t_total,t_reduce,samples_per_s,checksum\n";
    std::cout << std::setprecision(17) << index << ',' << experiment.group << ','
              << experiment.repeat << ',' << (experiment.sequential ? "seq" : "omp") << ','
              << mc::name(config.function) << ',' << mc::dimensions(config.function) << ','
              << (config.strata ? "strat" : "plain") << ',' << mode << ',' << samples << ','
              << config.seed << ',' << config.batch << ',' << config.threads << ',' << workers
              << ',' << config.max_samples;
    if (full) {
        const double exact = mc::exact(config.function);
        const double error = std::abs(estimate.estimate - exact);
        std::cout << ',' << estimate.estimate << ',' << exact << ',' << error << ','
                  << error / std::abs(exact) << ',' << estimate.std_err << ','
                  << estimate.ci_lo << ',' << estimate.ci_hi << ','
                  << (estimate.ci_lo <= exact && exact <= estimate.ci_hi) << ',';
        if (config.target_abs) std::cout << *config.target_abs;
        std::cout << ',' << estimate.reached;
    } else {
        // Ten fields from estimate through reached have no meaning for cost measurements.
        std::cout << ",,,,,,,,,,";
    }
    std::cout << ',' << total << ',' << reduce << ',' << samples / total << ',';
    if (!full) std::cout << benchmark.checksum;
    std::cout << '\n';
}
}

int main() {
    try {
        const auto cases = experiments();
        std::size_t index;
        // The Python runner selects a compiled test case, not application parameters.
        // Empty input lists cases. Every measurement runs in a fresh process.
        if (!(std::cin >> index)) {
            std::cout << "case_id,group,backend,mode\n";
            for (std::size_t i = 0; i < cases.size(); ++i)
                std::cout << i << ',' << cases[i].group << ','
                          << (cases[i].sequential ? "seq" : "omp") << ','
                          << mode_name(cases[i].config.mode) << '\n';
            return 0;
        }
        if (index >= cases.size()) throw std::invalid_argument("invalid experiment index");
        emit(cases[index], index);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
