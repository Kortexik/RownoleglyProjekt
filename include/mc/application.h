#pragma once

#include "mc/parameters.h"
#include "mc/result.h"

namespace mc {
using Integrator = Result (*)(const Config&);
using CostBenchmark = BenchmarkResult (*)(const Config&);

int run_application(int argc, const char* backend, Integrator integrate,
                    CostBenchmark benchmark);
} // namespace mc
