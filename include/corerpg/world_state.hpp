#pragma once

#include <corerpg/entity_id.hpp>

#include <cstdint>

namespace corerpg {

/// Owns the identities and mutation revision of the live campaign state.
class WorldState final {
public:
    using size_type = EntityId::value_type;
    using revision_type = std::uint64_t;

    /// Creates an entity, or returns an invalid ID if the ID space is exhausted.
    [[nodiscard]] EntityId create_entity() noexcept;

    [[nodiscard]] constexpr bool contains(EntityId entity_id) const noexcept {
        return entity_id.is_valid() && entity_id.value() <= last_entity_id_;
    }

    [[nodiscard]] constexpr size_type entity_count() const noexcept { return last_entity_id_; }
    [[nodiscard]] constexpr revision_type revision() const noexcept { return revision_; }

private:
    EntityId::value_type last_entity_id_{0};
    revision_type revision_{0};
};

} // namespace corerpg
