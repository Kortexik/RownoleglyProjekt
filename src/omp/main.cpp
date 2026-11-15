#include "mc/application.h"
#include "mc/openmp.h"

int main(int argc, char**) {
    return mc::run_application(argc, "omp", mc::integrate, mc::benchmark);
}
