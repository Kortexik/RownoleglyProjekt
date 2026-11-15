#pragma once

#include <Random123/philox.h>
#include <array>
#include <cstdint>

namespace mc {
inline std::array<std::uint32_t, 4> philox(std::uint64_t seed,
                                         std::uint64_t index,
                                         std::uint32_t coordinate) {
    const r123::Philox4x32::ctr_type counter = {{
        static_cast<std::uint32_t>(index), static_cast<std::uint32_t>(index >> 32),
        coordinate, 0}};
    const r123::Philox4x32::key_type key = {{
        static_cast<std::uint32_t>(seed), static_cast<std::uint32_t>(seed >> 32)}};
    const auto words = r123::Philox4x32{}(counter, key);
    return {words[0], words[1], words[2], words[3]};
}

inline double uniform(std::uint64_t seed, std::uint64_t index,
                      std::uint32_t coordinate) {
    const auto words = philox(seed, index, coordinate);
    // Exactly 53 random bits; unlike division by UINT64_MAX this cannot return 1.
    const std::uint64_t bits = (static_cast<std::uint64_t>(words[0]) << 21)
                               | (words[1] >> 11);
    return static_cast<double>(bits) * 0x1.0p-53;
}
} // namespace mc
