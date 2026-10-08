#pragma once

#include <corerpg/entity_id.hpp>

#include <cstdint>
#include <limits>

namespace corerpg::detail {

// Internal production transition, also exercised directly at numeric boundaries.
[[nodiscard]] constexpr EntityId advance_entity_creation(EntityId::value_type& last_entity_id,
                                                        std::uint64_t& revision) noexcept {
    if (last_entity_id == std::numeric_limits<EntityId::value_type>::max() ||
        revision == std::numeric_limits<std::uint64_t>::max()) {
        return EntityId{};
    }

    ++last_entity_id;
    ++revision;
    return EntityId{last_entity_id};
}

} // namespace corerpg::detail
