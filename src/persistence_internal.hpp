#pragma once

#include <corerpg/persistence.hpp>

#include <array>
#include <algorithm>
#include <climits>
#include <cstddef>
#include <limits>
#include <optional>

namespace corerpg::detail {

static_assert(CHAR_BIT == 8);
static_assert(std::numeric_limits<std::size_t>::digits <= 64);
inline constexpr std::array<std::uint8_t, 8> save_magic{'C', 'R', 'P', 'G', 'S', 'A', 'V', 'E'};
inline constexpr std::size_t save_header_size = 56;
inline constexpr std::size_t save_minimum_size = 60;

[[nodiscard]] inline std::uint32_t save_checksum(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ ((crc & 1U) ? 0x82F63B78U : 0U);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

// Bound arithmetic before addition/multiplication, including on 32-bit hosts.
[[nodiscard]] inline std::optional<std::size_t> save_size(std::uint64_t links,
                                                        std::uint64_t streams,
                                                        PersistenceLimits limits) noexcept {
    const auto maximum = std::min(limits.max_bytes,
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
    if (links > limits.max_links || streams > limits.max_streams ||
        maximum < save_minimum_size) {
        return std::nullopt;
    }
    const auto record_capacity = (maximum - save_minimum_size) / 16U;
    if (links > record_capacity || streams > record_capacity - links) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(save_minimum_size + 16U * (links + streams));
}

inline void append_integer(std::vector<std::uint8_t>& bytes, std::uint64_t value,
                           unsigned width) {
    for (unsigned i = 0; i < width; ++i) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        value >>= 8U;
    }
}

} // namespace corerpg::detail
