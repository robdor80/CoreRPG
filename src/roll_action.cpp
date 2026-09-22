#include <corerpg/roll_action.hpp>

#include <corerpg/random.hpp>
#include <corerpg/world_state.hpp>

namespace corerpg {

RollResult resolve_roll(const WorldState& world, DeterministicRandom& random,
                        const RollAction& action) noexcept {
    if (!world.contains(action.entity_id)) {
        return RollResult{RollStatus::entity_not_found, 0};
    }

    const auto value = random.uniform_closed(action.minimum, action.maximum);
    if (!value.has_value()) {
        return RollResult{RollStatus::invalid_range, 0};
    }

    return RollResult{RollStatus::success, *value};
}

} // namespace corerpg
