#pragma once

#include <corerpg/entity_id.hpp>

#include <compare>
#include <cstdint>

namespace corerpg {

/// Stable, caller-assigned label scoped to the campaign. Zero is invalid.
/// No family, authority, direction reversal or symmetry is implied by the label.
class RelationKind final {
  public:
    constexpr RelationKind() noexcept = default;
    constexpr explicit RelationKind(std::uint64_t value) noexcept : value_{value} {}
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value_ != 0;
    }
    [[nodiscard]] constexpr std::uint64_t value() const noexcept {
        return value_;
    }
    constexpr auto operator<=>(const RelationKind&) const noexcept = default;

  private:
    std::uint64_t value_{};
};

/// Directed edge. Canonical order is (source, kind, target); cycles/self edges are valid.
struct EntityRelation final {
    EntityId source;
    RelationKind kind;
    EntityId target;
    auto operator<=>(const EntityRelation&) const noexcept = default;
};

enum class DefinitionBindingChange : std::uint8_t {
    applied,
    no_change,
    entity_not_found,
    definition_not_found,
    definition_conflict,
    revision_exhausted,
};

enum class RelationChange : std::uint8_t {
    applied,
    no_change,
    source_not_found,
    target_not_found,
    invalid_kind,
    revision_exhausted,
};

/// A declared logical reserve. total is immutable; latent decreases on individualization.
struct PopulationRecord final {
    EntityId group;
    std::uint64_t total{};
    std::uint64_t latent{};
    auto operator<=>(const PopulationRecord&) const noexcept = default;
};

/// Immutable origin of an individualized entity, not current organizational membership.
struct PopulationMembership final {
    EntityId entity;
    EntityId population;
    auto operator<=>(const PopulationMembership&) const noexcept = default;
};

enum class PopulationChange : std::uint8_t {
    applied,
    no_change,
    group_not_found,
    population_conflict,
    revision_exhausted,
};

enum class MaterializationStatus : std::uint8_t {
    applied,
    no_change,
    group_not_found,
    population_not_found,
    batch_limit_exceeded,
    insufficient_latent,
    identity_exhausted,
    revision_exhausted,
};

/// Successful batch issues the contiguous range [first, first + count - 1].
/// Zero count has an invalid first; no result owns another world.
struct MaterializationResult final {
    MaterializationStatus status{MaterializationStatus::no_change};
    EntityId first;
    std::uint64_t count{};
};

} // namespace corerpg
