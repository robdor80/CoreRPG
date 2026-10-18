#pragma once

#include <corerpg/persistence.hpp>

#include <algorithm>
#include <array>
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
inline constexpr std::size_t save_v2_header_size = 88;
inline constexpr std::size_t save_v2_minimum_size = 92;
inline constexpr std::size_t save_v3_header_size = 96;
inline constexpr std::size_t save_v3_minimum_size = 100;
inline constexpr std::size_t knowledge_record_size = 62;

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
[[nodiscard]] inline std::optional<std::size_t>
save_size(std::uint64_t links, std::uint64_t streams, PersistenceLimits limits) noexcept {
    const auto maximum = std::min(
        limits.max_bytes, static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
    if (links > limits.max_links || streams > limits.max_streams || maximum < save_minimum_size) {
        return std::nullopt;
    }
    const auto record_capacity = (maximum - save_minimum_size) / 16U;
    if (links > record_capacity || streams > record_capacity - links) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(save_minimum_size + 16U * (links + streams));
}

inline void append_integer(std::vector<std::uint8_t>& bytes, std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
        value >>= 8U;
    }
}

[[nodiscard]] inline std::optional<std::size_t>
save_v2_minimum_size_for(std::uint64_t links, std::uint64_t streams, std::uint64_t definitions,
                         std::uint64_t relations, std::uint64_t populations,
                         std::uint64_t memberships, PersistenceLimits limits) noexcept {
    const auto maximum = std::min(
        limits.max_bytes, static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
    if (maximum < save_v2_minimum_size || links > limits.max_links ||
        streams > limits.max_streams || definitions > limits.max_definitions ||
        relations > limits.max_relations || populations > limits.max_populations ||
        memberships > limits.max_memberships) {
        return std::nullopt;
    }
    std::uint64_t size = save_v2_minimum_size;
    const auto add = [&](std::uint64_t count, std::uint64_t width) {
        if (count > (maximum - size) / width) {
            return false;
        }
        size += count * width;
        return true;
    };
    if (!add(links, 16) || !add(streams, 16) || !add(definitions, 12) || !add(relations, 24) ||
        !add(populations, 24) || !add(memberships, 16)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(size);
}

[[nodiscard]] inline std::optional<std::size_t>
save_v3_minimum_size_for(std::uint64_t links, std::uint64_t streams, std::uint64_t definitions,
                         std::uint64_t relations, std::uint64_t populations,
                         std::uint64_t memberships, std::uint64_t knowledge,
                         PersistenceLimits limits) noexcept {
    const auto base = save_v2_minimum_size_for(links, streams, definitions, relations, populations,
                                               memberships, limits);
    const auto maximum = std::min(limits.max_bytes,
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
    if (!base || maximum - *base < 8 || knowledge > limits.max_knowledge ||
        knowledge > (maximum - *base - 8) / knowledge_record_size) return std::nullopt;
    return static_cast<std::size_t>(*base + 8 + knowledge * knowledge_record_size);
}

[[nodiscard]] inline std::optional<std::size_t>
encoded_save_size(const CampaignSnapshot& snapshot, PersistenceLimits limits) noexcept {
    if (snapshot.format_version == save_format_version) {
        return save_size(snapshot.containment.size(), snapshot.streams.size(), limits);
    }
    auto size = save_v2_minimum_size_for(snapshot.containment.size(), snapshot.streams.size(),
                                         snapshot.definitions.size(), snapshot.relations.size(),
                                         snapshot.populations.size(),
                                         snapshot.population_memberships.size(), limits);
    if (snapshot.format_version == save_format_version_v3)
        size = save_v3_minimum_size_for(snapshot.containment.size(), snapshot.streams.size(),
            snapshot.definitions.size(), snapshot.relations.size(), snapshot.populations.size(),
            snapshot.population_memberships.size(), snapshot.knowledge.size(), limits);
    if (!size) {
        return std::nullopt;
    }
    const auto maximum = std::min(
        limits.max_bytes, static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()));
    for (const auto& binding : snapshot.definitions) {
        const auto length = binding.definition_id.size();
        if (length > limits.max_definition_bytes || length > UINT32_MAX ||
            length > maximum - *size) {
            return std::nullopt;
        }
        *size += length;
    }
    for (const auto& record : snapshot.knowledge) {
        const auto length = record.fact.definition_id().size();
        if (length > limits.max_definition_bytes || length > UINT32_MAX || length > maximum - *size)
            return std::nullopt;
        *size += length;
    }
    return size;
}

} // namespace corerpg::detail
