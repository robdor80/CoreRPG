#include <corerpg/world_state.hpp>

#include <limits>

namespace corerpg {

EntityId WorldState::create_entity() noexcept {
    if (last_entity_id_ == std::numeric_limits<EntityId::value_type>::max()) {
        return EntityId{};
    }

    ++last_entity_id_;
    ++revision_;
    return EntityId{last_entity_id_};
}

} // namespace corerpg
