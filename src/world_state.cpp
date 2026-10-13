#include <corerpg/world_state.hpp>

#include "world_state_internal.hpp"

namespace corerpg {

EntityId WorldState::create_entity() noexcept {
    return detail::advance_entity_creation(last_entity_id_, revision_);
}

ContainmentResult WorldState::container_of(EntityId entity) const noexcept {
    if (!contains(entity)) {
        return {};
    }
    const auto link = detail::find_containment(containment_, entity);
    if (link == containment_.end() || link->first != entity) {
        return {ContainmentStatus::uncontained, EntityId{}};
    }
    return {ContainmentStatus::contained, link->second};
}

ContainmentChange WorldState::place_entity(EntityId entity, EntityId container) {
    return detail::place_entity(containment_, last_entity_id_, revision_, entity, container);
}

ContainmentChange WorldState::remove_containment(EntityId entity) noexcept {
    return detail::remove_containment(containment_, last_entity_id_, revision_, entity);
}

} // namespace corerpg
