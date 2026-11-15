#include "mc/openmp.h"
#include "mc/sequential.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void close(double actual, double expected) {
    check(std::isfinite(actual) && std::abs(actual - expected) < 1e-12,
          "sequential and OpenMP estimates or intervals differ");
}

void compare(const mc::Config& config) {
    const auto sequential = mc::seq::integrate(config);
    const auto parallel = mc::integrate(config);
    check(sequential.workers == 1, "sequential run reports multiple workers");
    check(sequential.samples == parallel.samples, "different sample budgets or stopping rounds");
    check(sequential.reached == parallel.reached, "different stopping decisions");
    close(sequential.estimate, parallel.estimate);
    close(sequential.std_err, parallel.std_err);
    close(sequential.ci_lo, parallel.ci_lo);
    close(sequential.ci_hi, parallel.ci_hi);
    check(std::abs(sequential.estimate - mc::exact(config.function))
              < 6 * sequential.std_err, "sequential estimate disagrees with known integral");
}

void integration() {
    for (auto function : {mc::Function::Sin, mc::Function::Gauss1d,
                          mc::Function::Peak, mc::Function::GaussNd})
    for (unsigned strata : {0U, 64U}) {
        if (strata && function == mc::Function::GaussNd) continue;
        for (unsigned seed : {1U, 42U})
        for (int threads : {1, 4}) {
            mc::Config config;
            config.function = function;
            config.strata = strata;
            config.seed = seed;
            config.threads = threads;
            config.samples = 10003;
            config.batch = 511;
            const auto fixed = mc::seq::integrate(config);
            check(fixed.samples == 10003 && !fixed.reached, "fixed budget not respected");
            compare(config);

            config.target_abs = 0.001;
            config.max_samples = 100003;
            config.batch = 4096;
            compare(config);

            config.target_abs = 1e-15;
            config.max_samples = 10003;
            config.batch = 1000;
            const auto limited = mc::seq::integrate(config);
            check(limited.samples == 10003 && !limited.reached, "sample limit not respected");
            compare(config);
        }
    }
}

void benchmarks() {
    for (auto function : {mc::Function::Sin, mc::Function::Gauss1d,
                          mc::Function::Peak, mc::Function::GaussNd})
    for (auto mode : {mc::Mode::RngOnly, mc::Mode::FuncOnly}) {
        mc::Config config;
        config.function = function;
        config.mode = mode;
        config.samples = 10003;
        const auto sequential = mc::seq::benchmark(config);
        const auto parallel = mc::benchmark(config);
        check(sequential.workers == 1 && sequential.samples == 10003,
              "wrong sequential benchmark execution metadata");
        check(std::abs(sequential.checksum - parallel.checksum) < 1e-8,
              "sequential and parallel benchmark checksums differ");
    }
}

template<class Action>
void rejects(Action action, const char* message) {
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}

void invalid_modes() {
    mc::Config config;
    config.mode = mc::Mode::RngOnly;
    rejects([&] { mc::seq::integrate(config); },
            "sequential integration accepts benchmark mode");
    config.mode = mc::Mode::Full;
    rejects([&] { mc::seq::benchmark(config); },
            "sequential benchmark accepts integration mode");
}
}

int main() {
    try {
        integration();
        benchmarks();
        invalid_modes();
        std::cout << "Sequential and OpenMP results agree\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
