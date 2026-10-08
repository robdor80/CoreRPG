#pragma once

#include <corerpg/entity_id.hpp>

#include <cstdint>
#include <optional>

namespace corerpg {

/// Owns the identities and mutation revision of the live campaign state.
class WorldState final {
public:
    using size_type = EntityId::value_type;
    using revision_type = std::uint64_t;

    /// Creates an entity, or returns an invalid ID if IDs or revisions are exhausted.
    /// Rejection leaves the world unchanged.
    [[nodiscard]] EntityId create_entity() noexcept;

    [[nodiscard]] constexpr bool contains(EntityId entity_id) const noexcept {
        return entity_id.is_valid() && entity_id.value() <= last_entity_id_;
    }

    [[nodiscard]] constexpr size_type entity_count() const noexcept { return last_entity_id_; }

    /// Returns the least existing ID greater than after, or nullopt at the end.
    /// Zero starts traversal; the cursor need not belong to this world.
    /// Successive queries are not a snapshot if mutations occur between them.
    [[nodiscard]] constexpr std::optional<EntityId> next_entity_id(EntityId after) const noexcept {
        if (after.value() >= last_entity_id_) {
            return std::nullopt;
        }
        return EntityId{after.value() + 1U};
    }

    [[nodiscard]] constexpr revision_type revision() const noexcept { return revision_; }

private:
    EntityId::value_type last_entity_id_{0};
    revision_type revision_{0};
};

} // namespace corerpg
