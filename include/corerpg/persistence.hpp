#pragma once

#include <corerpg/random_streams.hpp>
#include <corerpg/world_state.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace corerpg {

inline constexpr std::uint32_t save_format_version = 1;
inline constexpr std::uint32_t save_rng_policy = 1;
inline constexpr std::uint32_t population_save_format_version = 2;

/// Owned canonical text, resolved to a strong DefinitionId against the supplied catalog at load.
struct DefinitionBindingSnapshot final {
    EntityId entity;
    std::string definition_id;
    DefinitionBindingSnapshot() : definition_id{"", 0} {}
    DefinitionBindingSnapshot(EntityId id, std::string_view definition)
        : entity{id}, definition_id{definition.data(), definition.size()} {}
    DefinitionBindingSnapshot(const DefinitionBindingSnapshot&) = default;
    DefinitionBindingSnapshot& operator=(const DefinitionBindingSnapshot&) = default;
    DefinitionBindingSnapshot& operator=(DefinitionBindingSnapshot&&) = default;
    // Copy string construction propagates MSVC debug-proxy failures, unlike string move.
    DefinitionBindingSnapshot(DefinitionBindingSnapshot&& other)
        : entity{other.entity}, definition_id{other.definition_id} {}
    bool operator==(const DefinitionBindingSnapshot&) const = default;
};

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
    // Count constructors can propagate MSVC debug-proxy allocation failures;
    // default vector construction and vector moves are noexcept on that STL.
    std::vector<ContainmentSnapshot> containment = std::vector<ContainmentSnapshot>(0);
    std::vector<RandomStreamSnapshot> streams = std::vector<RandomStreamSnapshot>(0);
    std::vector<DefinitionBindingSnapshot> definitions = std::vector<DefinitionBindingSnapshot>(0);
    std::vector<EntityRelation> relations = std::vector<EntityRelation>(0);
    std::vector<PopulationRecord> populations = std::vector<PopulationRecord>(0);
    std::vector<PopulationMembership> population_memberships = std::vector<PopulationMembership>(0);

    CampaignSnapshot() = default;
    CampaignSnapshot(const CampaignSnapshot&) = default;
    CampaignSnapshot& operator=(const CampaignSnapshot&) = default;
    CampaignSnapshot& operator=(CampaignSnapshot&&) = default;
    CampaignSnapshot(CampaignSnapshot&& other)
        : format_version{other.format_version}, rng_policy{other.rng_policy},
          last_entity_id{other.last_entity_id}, revision{other.revision},
          master_seed{other.master_seed} {
        containment.swap(other.containment);
        streams.swap(other.streams);
        definitions.swap(other.definitions);
        relations.swap(other.relations);
        populations.swap(other.populations);
        population_memberships.swap(other.population_memberships);
    }
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
    unexpected_fields,
};

struct SerializedSnapshot final {
    PersistenceStatus status{PersistenceStatus::success};
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0);

    SerializedSnapshot(PersistenceStatus result = PersistenceStatus::success) : status{result} {}
    SerializedSnapshot(PersistenceStatus result, std::vector<std::uint8_t>&& data)
        : status{result} {
        bytes.swap(data);
    }
    SerializedSnapshot(const SerializedSnapshot&) = default;
    SerializedSnapshot& operator=(const SerializedSnapshot&) = default;
    SerializedSnapshot& operator=(SerializedSnapshot&&) = default;
    SerializedSnapshot(SerializedSnapshot&& other) : status{other.status} {
        bytes.swap(other.bytes);
    }
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
