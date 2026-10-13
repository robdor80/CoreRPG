#pragma once

#include <corerpg/entity_id.hpp>
#include <corerpg/entity_structure.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace corerpg {

struct CampaignSnapshot;
class ContentCatalog;
class DefinitionId;

enum class ContainmentStatus : std::uint8_t {
    entity_not_found,
    uncontained,
    contained,
};

struct ContainmentResult final {
    ContainmentStatus status{ContainmentStatus::entity_not_found};
    EntityId container{};
};

enum class ContainmentChange : std::uint8_t {
    applied,
    no_change,
    entity_not_found,
    container_not_found,
    self_containment,
    cycle_detected,
    revision_exhausted,
};

/// Owns the identities and mutation revision of the live campaign state.
class WorldState final {
  public:
    using size_type = EntityId::value_type;
    using revision_type = std::uint64_t;

    constexpr WorldState() noexcept = default;
    constexpr WorldState(const WorldState&) = default;
    constexpr WorldState(WorldState&&) noexcept = default;
    constexpr WorldState& operator=(WorldState&&) noexcept = default;

    /// Replaces the whole value; allocation failure preserves the previous world.
    constexpr WorldState& operator=(const WorldState& other) {
        if (this != &other) {
            WorldState replacement{other};
            swap_state(replacement);
        }
        return *this;
    }

    /// Creates an entity, or returns an invalid ID if IDs or revisions are exhausted.
    /// Rejection leaves the world unchanged.
    [[nodiscard]] EntityId create_entity() noexcept;

    [[nodiscard]] constexpr bool contains(EntityId entity_id) const noexcept {
        return entity_id.is_valid() && entity_id.value() <= last_entity_id_;
    }

    [[nodiscard]] constexpr size_type entity_count() const noexcept {
        return last_entity_id_;
    }

    /// Returns the least existing ID greater than after, or nullopt at the end.
    /// Zero starts traversal; the cursor need not belong to this world.
    /// Successive queries are not a snapshot if mutations occur between them.
    [[nodiscard]] constexpr std::optional<EntityId> next_entity_id(EntityId after) const noexcept {
        if (after.value() >= last_entity_id_) {
            return std::nullopt;
        }
        return EntityId{after.value() + 1U};
    }

    [[nodiscard]] constexpr revision_type revision() const noexcept {
        return revision_;
    }

    /// Distinguishes a missing entity, no registered link, and a direct container.
    /// The container is invalid unless status is contained.
    [[nodiscard]] ContainmentResult container_of(EntityId entity) const noexcept;

    /// Validates entity, container, self-link, cycle, no-op, then revision, in that order.
    /// Allocation failures propagate and preserve the entire previous world.
    [[nodiscard]] ContainmentChange place_entity(EntityId entity, EntityId container);

    /// Removes only this entity's direct link; preserves entities and descendants.
    [[nodiscard]] ContainmentChange remove_containment(EntityId entity) noexcept;

    /// Optional immutable association to an existing declarative definition, not payload
    /// generation.
    [[nodiscard]] DefinitionBindingChange
    bind_definition(EntityId entity, const ContentCatalog& catalog, const DefinitionId& definition);
    [[nodiscard]] const DefinitionId* definition_of(EntityId entity) const noexcept;

    [[nodiscard]] RelationChange add_relation(EntityRelation relation);
    [[nodiscard]] RelationChange remove_relation(EntityRelation relation) noexcept;
    [[nodiscard]] bool has_relation(EntityRelation relation) const noexcept;
    /// Borrowed canonical views; a relation mutation may invalidate them.
    [[nodiscard]] std::span<const EntityRelation> relations() const noexcept {
        return relations_;
    }
    [[nodiscard]] std::span<const EntityRelation> relations_from(EntityId source) const noexcept;

  private:
    friend class Persistence;
    /// Private validated staging path; may throw without touching an existing world.
    explicit WorldState(const CampaignSnapshot& snapshot);
    constexpr void swap_state(WorldState& other) noexcept {
        std::swap(last_entity_id_, other.last_entity_id_);
        std::swap(revision_, other.revision_);
        containment_.swap(other.containment_);
        definitions_.swap(other.definitions_);
        relations_.swap(other.relations_);
        populations_.swap(other.populations_);
        population_memberships_.swap(other.population_memberships_);
    }
    EntityId::value_type last_entity_id_{0};
    revision_type revision_{0};
    std::vector<std::pair<EntityId, EntityId>> containment_;
    // Shared values are immutable; each world's binding table has independent value semantics.
    std::vector<std::pair<EntityId, std::shared_ptr<const DefinitionId>>> definitions_;
    std::vector<EntityRelation> relations_;
    std::vector<PopulationRecord> populations_;
    std::vector<PopulationMembership> population_memberships_;
};

} // namespace corerpg
