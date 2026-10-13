#pragma once

#include <corerpg/random_streams.hpp>
#include <corerpg/world_state.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace corerpg {

inline constexpr std::uint32_t save_format_version = 1;
inline constexpr std::uint32_t save_rng_policy = 1;

struct ContainmentSnapshot final {
    EntityId entity;
    EntityId container;
    bool operator==(const ContainmentSnapshot&) const = default;
};

struct RandomStreamSnapshot final {
    RandomStreams::stream_id_type id{};
    DeterministicRandom::state_type state{};
    bool operator==(const RandomStreamSnapshot&) const = default;
};

/// Detached candidate data, never a second authoritative world.
/// Version 1 represents exactly the issued IDs [1, last_entity_id], without gaps.
struct CampaignSnapshot final {
    std::uint32_t format_version{save_format_version};
    std::uint32_t rng_policy{save_rng_policy};
    EntityId::value_type last_entity_id{};
    WorldState::revision_type revision{};
    RandomStreams::seed_type master_seed{};
    std::vector<ContainmentSnapshot> containment;
    std::vector<RandomStreamSnapshot> streams;
    bool operator==(const CampaignSnapshot&) const = default;
};

/// Resource policy, not gameplay limits. Applied before allocating decoded records.
struct PersistenceLimits final {
    std::uint64_t max_bytes{64ULL * 1024ULL * 1024ULL};
    std::uint64_t max_links{1'000'000};
    std::uint64_t max_streams{1'000'000};
};

enum class PersistenceStatus : std::uint8_t {
    success,
    invalid_magic,
    unsupported_version,
    unsupported_rng_policy,
    invalid_size,
    checksum_mismatch,
    limit_exceeded,
    invalid_world,
    invalid_link,
    noncanonical_links,
    cycle_detected,
    noncanonical_streams,
};

struct SerializedSnapshot final {
    PersistenceStatus status{PersistenceStatus::success};
    std::vector<std::uint8_t> bytes;
};

struct DecodedSnapshot final {
    PersistenceStatus status{PersistenceStatus::success};
    std::optional<CampaignSnapshot> snapshot;
};

/// Core-owned persistence boundary. No filesystem, mutable storage access or RNG draws.
/// Allocation exceptions propagate, preserving all input and destination objects.
/// Callers must exclude concurrent mutations throughout capture/restore/load.
class Persistence final {
public:
    [[nodiscard]] static CampaignSnapshot capture(const WorldState& world,
                                                  const RandomStreams& random);
    [[nodiscard]] static PersistenceStatus validate(const CampaignSnapshot& snapshot,
                                                    PersistenceLimits limits = {});
    [[nodiscard]] static SerializedSnapshot serialize(const CampaignSnapshot& snapshot,
                                                       PersistenceLimits limits = {});
    [[nodiscard]] static DecodedSnapshot deserialize(std::span<const std::uint8_t> bytes,
                                                      PersistenceLimits limits = {});
    /// Replaces both destinations together; success invalidates old stream references.
    [[nodiscard]] static PersistenceStatus restore(const CampaignSnapshot& snapshot,
                                                   WorldState& world, RandomStreams& random,
                                                   PersistenceLimits limits = {});
    [[nodiscard]] static PersistenceStatus load(std::span<const std::uint8_t> bytes,
                                                WorldState& world, RandomStreams& random,
                                                PersistenceLimits limits = {});
};

} // namespace corerpg
