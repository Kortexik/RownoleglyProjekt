#pragma once

#include "mc/parameters.h"

namespace mc {
// Wspolne ustawienia eksperymentu. Zmieniaj wartosci tylko tutaj.
inline constexpr Function FUNCTION = Function::Gauss1d;
inline constexpr Mode MODE = Mode::Full;
inline constexpr std::uint64_t SAMPLES = 1'000'000;
inline constexpr std::uint64_t SEED = 42;
inline constexpr std::uint64_t BATCH = 65'536;
inline constexpr std::uint64_t MAX_SAMPLES = 100'000'000;
inline constexpr std::optional<double> TARGET_ABS = std::nullopt; // Np. 0.001.
inline constexpr int THREADS = 4; // 0 = ustawienie OpenMP.
inline constexpr unsigned STRATA = 0; // 0 = zwykle MC, 64 = warstwy w 1D.
inline constexpr char CSV_PATH[] = ""; // Np. "wyniki.csv"; puste = tylko ekran.

inline constexpr Config CONFIG{
    FUNCTION, MODE, SAMPLES, SEED, BATCH, MAX_SAMPLES, TARGET_ABS, THREADS, STRATA
};
} // namespace mc
