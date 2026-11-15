#include "mc/functions.h"
#include "mc/openmp.h"
#include "mc/philox.h"
#include "mc/stats.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void close(double actual, double expected, double tolerance, const std::string& message) {
    check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}

template<class F> void rejects(F action, const std::string& message) {
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}

void statistics() {
    mc::Stats whole, left, right;
    const double values[] = {2, 4, 4, 4, 5, 5, 7, 9};
    for (int i = 0; i < 8; ++i) {
        whole.add(values[i]);
        (i < 3 ? left : right).add(values[i]);
    }
    left.merge(right);
    left.merge(mc::Stats{});
    check(left.n == 8, "merge loses samples");
    close(whole.mean, 5, 1e-14, "wrong Welford mean");
    close(whole.m2, 32, 1e-13, "wrong Welford M2");
    close(left.mean, 5, 1e-14, "wrong merged mean");
    close(left.m2, 32, 1e-13, "wrong merged M2");
    close(left.variance(), 32.0 / 7, 1e-13, "variance needs n-1 divisor");
}

void generator() {
    const auto words = mc::philox(0, 0, 0);
    const std::array<std::uint32_t, 4> expected = {
        0x6627e8d5U, 0xe169c58dU, 0xbc57ac4cU, 0x9b00dbd8U};
    check(words == expected, "Philox fails published known-answer vector");
    for (std::uint64_t i = 0; i < 1000; ++i) {
        const double u = mc::uniform(42, i, 0);
        check(u >= 0 && u < 1, "uniform sample outside [0,1)");
    }
    check(mc::uniform(42, 7, 0) != mc::uniform(43, 7, 0), "seed ignored");
    check(mc::uniform(42, 7, 0) != mc::uniform(42, 7, 1), "coordinate ignored");
    check(mc::uniform(42, 7, 0) != mc::uniform(42, (1ULL << 32) + 7, 0),
          "upper counter bits ignored");
}

void integral_and_partition() {
    for (const auto function : {mc::Function::Sin, mc::Function::Gauss1d,
                               mc::Function::Peak, mc::Function::GaussNd}) {
        mc::Config cfg;
        cfg.function = function;
        cfg.samples = 100003;
        cfg.batch = 7001;
        cfg.threads = 1;
        const auto one = mc::integrate(cfg);
        mc::Stats reference;
        for (std::uint64_t i = 0; i < cfg.samples; ++i)
            reference.add(mc::sample(function, cfg.seed, i));
        close(one.estimate, reference.mean, 1e-12, "round repeats or skips samples");
        cfg.threads = 4;
        const auto four = mc::integrate(cfg);
        check(four.samples == cfg.samples, "partial final round has wrong count");
        check(four.workers >= 1 && four.workers <= 4, "invalid actual team size");
        close(four.estimate, one.estimate, 1e-12, "thread count changes sample set");
        close(four.std_err, one.std_err, 1e-12, "thread count changes variance");
        close(four.estimate, mc::exact(function), 6 * four.std_err,
              "estimate inconsistent with known integral");
        close(four.ci_hi - four.estimate, 1.96 * four.std_err, 1e-14,
              "wrong confidence interval");
    }
    close(mc::exact(mc::Function::Sin), 2, 1e-15, "wrong sin integral");
    close(mc::exact(mc::Function::Gauss1d), 0.746824132812427, 1e-14,
          "wrong Gaussian integral");
    close(mc::exact(mc::Function::Peak), 0.274680153389003, 1e-14,
          "wrong peak integral");
    close(mc::exact(mc::Function::GaussNd), 0.3110809188228766, 1e-14,
          "wrong 4D Gaussian integral");
}

void stratification() {
    mc::Config cfg;
    cfg.strata = 64;
    cfg.samples = 10003;
    cfg.batch = 1; // Some early rounds have empty strata.
    cfg.threads = 1;
    const auto strat = mc::integrate(cfg);
    std::array<mc::Stats, 64> reference{};
    for (std::uint64_t i = 0; i < cfg.samples; ++i)
        reference[i % 64].add(mc::sample(cfg.function, cfg.seed, i, 64));
    double mean = 0, variance = 0;
    for (const auto& s : reference) {
        mean += s.mean / 64;
        variance += s.variance() / static_cast<double>(s.n) / 4096;
    }
    close(strat.estimate, mean, 1e-12, "stratified mean uses wrong weights");
    close(strat.std_err, std::sqrt(variance), 1e-12, "wrong stratified variance");
    cfg.batch = 999;
    cfg.threads = 4;
    const auto parallel = mc::integrate(cfg);
    close(parallel.estimate, strat.estimate, 1e-12, "strata depend on rounds or threads");
    cfg.strata = 0;
    const auto plain = mc::integrate(cfg);
    check(strat.std_err < plain.std_err / 10, "stratification does not reduce error");
}

void stopping() {
    mc::Config cfg;
    cfg.threads = 2;
    cfg.target_abs = 0.1;
    cfg.max_samples = 10000;
    cfg.batch = 1000;
    const auto reached = mc::integrate(cfg);
    check(reached.reached && reached.samples == 5000,
          "stopping ignores minimum sample count or round boundary");
    cfg.target_abs = 1e-15;
    cfg.max_samples = 10003;
    const auto limited = mc::integrate(cfg);
    check(!limited.reached && limited.samples == 10003, "sample limit overshot");
    cfg.strata = 64;
    cfg.target_abs = 0.001;
    cfg.max_samples = 20000;
    const auto strat = mc::integrate(cfg);
    check(strat.reached && strat.ci_hi - strat.estimate <= 0.001,
          "stratified stopping ignores stratified interval");
    cfg.max_samples = 2000;
    const auto too_few = mc::integrate(cfg);
    check(!too_few.reached && too_few.samples == 2000, "minimum sample guard ignored");
}

void cost_benchmarks() {
    mc::Config cfg;
    cfg.mode = mc::Mode::RngOnly;
    cfg.samples = 10003;
    cfg.batch = 511;
    cfg.threads = 4;
    for (auto function : {mc::Function::Gauss1d, mc::Function::GaussNd}) {
        cfg.function = function;
        double expected = 0;
        for (std::uint64_t i = 0; i < cfg.samples; ++i)
            for (unsigned d = 0; d < mc::dimensions(function); ++d)
                expected += mc::uniform(cfg.seed, i, d);
        const auto result = mc::benchmark(cfg);
        check(result.samples == cfg.samples, "benchmark loses samples");
        close(result.checksum, expected, 1e-8, "wrong parallel RNG checksum");
    }
    cfg.function = mc::Function::Gauss1d;
    cfg.mode = mc::Mode::FuncOnly;
    cfg.samples = 1048576;
    const auto result = mc::benchmark(cfg);
    // Midpoint quadrature of exp(-x*x) on [0,1]; independent analytic reference.
    close(result.checksum, 1048576 * 0.746824132812427, 1e-6,
          "function benchmark evaluates wrong points or function");
}

void invalid_configuration() {
    mc::Config cfg;
    cfg.batch = 0;
    rejects([&] { mc::integrate(cfg); }, "zero batch accepted");
    cfg.batch = 65536;
    cfg.strata = 64;
    cfg.samples = 127;
    rejects([&] { mc::integrate(cfg); }, "insufficient samples per stratum accepted");
    cfg.samples = 1000;
    cfg.function = mc::Function::GaussNd;
    rejects([&] { mc::integrate(cfg); }, "multidimensional stratification accepted");
    cfg = mc::Config{};
    cfg.mode = mc::Mode::RngOnly;
    rejects([&] { mc::integrate(cfg); }, "integration accepts a benchmark mode");
    cfg.target_abs = 0.001;
    rejects([&] { mc::benchmark(cfg); }, "benchmark accepts an accuracy target");
    cfg = mc::Config{};
    rejects([&] { mc::benchmark(cfg); }, "benchmark accepts Full mode");
}
} // namespace

int main() {
    try {
        statistics();
        generator();
        integral_and_partition();
        stratification();
        stopping();
        cost_benchmarks();
        invalid_configuration();
        std::cout << "All numerical and OpenMP tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
