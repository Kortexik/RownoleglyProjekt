#include "mc/application.h"
#include "mc/sequential.h"

int main(int argc, char**) {
    return mc::run_application(argc, "seq", mc::seq::integrate, mc::seq::benchmark);
}
