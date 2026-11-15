#include "mc/config.h"
#include "mc/application.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
const std::string header =
    "backend,func,dim,method,mode,samples,seed,batch,workers,estimate,exact,abs_err,"
    "rel_err,std_err,ci_lo,ci_hi,ci_hit,target,reached,t_total,t_reduce,samples_per_s,checksum";

std::string number(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    return stream.str();
}

std::vector<std::string> csv_prefix(const mc::Config& cfg, std::uint64_t samples,
                                    int workers, const char* mode, const char* backend) {
    return {backend, mc::name(cfg.function), std::to_string(mc::dimensions(cfg.function)),
            cfg.strata ? "strat" : "plain", mode, std::to_string(samples),
            std::to_string(cfg.seed), std::to_string(cfg.batch), std::to_string(workers)};
}

std::string finish_csv(std::vector<std::string> cells, std::uint64_t samples,
                       double total, double reduce, const std::string& checksum) {
    cells.insert(cells.end(), {number(total), number(reduce),
        number(total > 0 ? static_cast<double>(samples) / total : 0), checksum});
    std::string row;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i) row += ',';
        row += cells[i];
    }
    return row;
}

std::string csv_row(const mc::Config& cfg, const mc::Result& result, const char* backend) {
    const double exact = mc::exact(cfg.function);
    const double error = std::abs(result.estimate - exact);
    auto cells = csv_prefix(cfg, result.samples, result.workers, "full", backend);
    cells.insert(cells.end(), {
        number(result.estimate), number(exact), number(error), number(error / std::abs(exact)),
        number(result.std_err), number(result.ci_lo), number(result.ci_hi),
        result.ci_lo <= exact && exact <= result.ci_hi ? "1" : "0",
        cfg.target_abs ? number(*cfg.target_abs) : "", result.reached ? "1" : "0"
    });
    return finish_csv(cells, result.samples, result.t_total, result.t_reduce, "");
}

std::string csv_row(const mc::Config& cfg, const mc::BenchmarkResult& result, const char* backend) {
    const char* mode = cfg.mode == mc::Mode::RngOnly ? "rng-only" : "func-only";
    auto cells = csv_prefix(cfg, result.samples, result.workers, mode, backend);
    constexpr std::size_t integral_columns = 10; // estimate through reached.
    cells.insert(cells.end(), integral_columns, "");
    return finish_csv(cells, result.samples, result.t_total, result.t_reduce,
                      number(result.checksum));
}

void append_csv(const std::string& path, const std::string& row) {
    const bool existing = std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
    if (existing) {
        std::ifstream input(path);
        std::string first_line;
        if (!std::getline(input, first_line) || first_line != header)
            throw std::runtime_error("plik CSV ma inny lub nieczytelny naglowek: " + path);
    }
    std::ofstream output(path, std::ios::app);
    if (!output) throw std::runtime_error("nie mozna otworzyc pliku CSV: " + path);
    if (!existing) output << header << '\n';
    output << row << '\n';
    output.flush();
    if (!output) throw std::runtime_error("nie mozna zapisac pliku CSV: " + path);
}
} // namespace

namespace mc {
int run_application(int argc, const char* backend, Integrator integrate,
                    CostBenchmark benchmark) {
    try {
        if (argc != 1)
            throw std::invalid_argument("Ustaw parametry w include/mc/config.h i uruchom bez argumentow");
        const auto& cfg = mc::CONFIG;
        const auto row = cfg.mode == mc::Mode::Full
            ? csv_row(cfg, integrate(cfg), backend)
            : csv_row(cfg, benchmark(cfg), backend);
        if (mc::CSV_PATH[0] != '\0') append_csv(mc::CSV_PATH, row);
        std::cout << header << '\n' << row << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Blad: " << error.what() << '\n';
        return 1;
    }
}
} // namespace mc
