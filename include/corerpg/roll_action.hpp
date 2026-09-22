#pragma once

#include <corerpg/entity_id.hpp>

#include <cstdint>

namespace corerpg {

class DeterministicRandom;
class WorldState;

struct RollAction final {
    EntityId entity_id{};
    std::uint64_t minimum{0};
    std::uint64_t maximum{0};
};

enum class RollStatus : std::uint8_t {
    success,
    entity_not_found,
    invalid_range,
};

struct RollResult final {
    RollStatus status{RollStatus::invalid_range};
    std::uint64_t value{0};

    [[nodiscard]] constexpr bool succeeded() const noexcept {
        return status == RollStatus::success;
    }
};

/// Resolves a roll without mutating the persistent world state.
[[nodiscard]] RollResult resolve_roll(const WorldState& world, DeterministicRandom& random,
                                      const RollAction& action) noexcept;

} // namespace corerpg
