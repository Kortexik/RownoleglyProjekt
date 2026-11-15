#include "mc/monte_carlo.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void close(double value, double expected) {
    if (!std::isfinite(value) || std::abs(value - expected) > 1e-12)
        throw std::runtime_error("wrong common estimator calculation");
}

void plain_estimate() {
    mc::StrataStats totals{};
    for (double value : {2, 4, 4, 4, 5, 5, 7, 9}) totals[0].add(value);
    mc::Config config;
    mc::Result result;
    result.workers = 3;
    mc::update_estimate(config, totals, result);
    close(result.estimate, 5);
    close(result.std_err, 0.7559289460184544);
    close(result.ci_hi, 6.481620734196171);
    if (result.samples != 8 || result.workers != 3)
        throw std::runtime_error("sample count or execution metadata overwritten");
}

void stratified_estimate() {
    mc::StrataStats totals{};
    for (unsigned k = 0; k < 64; ++k) {
        totals[k].add(k);
        totals[k].add(k + 2);
    }
    mc::Config config;
    config.strata = 64;
    mc::Result result;
    mc::update_estimate(config, totals, result);
    close(result.estimate, 32.5);
    close(result.std_err, 0.125);
    close(result.ci_lo, 32.255);
    if (result.samples != 128) throw std::runtime_error("wrong stratum sample count");
}

void accuracy_threshold() {
    mc::StrataStats totals{};
    for (unsigned i = 0; i < 4095; ++i) totals[0].add(i % 2 ? 2 : 0);
    mc::Config config;
    config.target_abs = 0.1;
    mc::Result result;
    mc::update_estimate(config, totals, result);
    if (result.reached) throw std::runtime_error("minimum accuracy sample count ignored");
    totals[0].add(2);
    mc::update_estimate(config, totals, result);
    if (!result.reached) throw std::runtime_error("accuracy threshold not reached");
}
}

int main() {
    try {
        plain_estimate();
        stratified_estimate();
        accuracy_threshold();
        std::cout << "Shared estimator tests passed without OpenMP\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
