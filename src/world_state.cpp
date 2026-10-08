#include <corerpg/world_state.hpp>

#include "world_state_internal.hpp"

namespace corerpg {

EntityId WorldState::create_entity() noexcept {
    return detail::advance_entity_creation(last_entity_id_, revision_);
}

} // namespace corerpg
