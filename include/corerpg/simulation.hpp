#pragma once

#include <corerpg/action.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace corerpg {

struct SimulationLimits final {
    std::size_t max_actions{64};
    ActionLimits action_limits{};
};

enum class SimulationStatus : std::uint8_t {
    completed,
    action_limit_exceeded,
    insufficient_output,
    allocation_failure,
    capacity_failure,
};

struct SimulationResult final {
    SimulationStatus status{SimulationStatus::completed};
    /// Only results[0..processed) have been written and are definitive, including
    /// rejected/no-op requests. Resume with actions.subspan(processed), never replay
    /// the prefix. The failed action and unprocessed suffix have no effects.
    std::size_t processed{};
    bool operator==(const SimulationResult&) const = default;
};

/// Stateless causal driver over the existing WorldState and RandomStreams.
/// One explicit step, input order, atomicity per action rather than per batch.
/// No queue, hidden history, clock, rendering, allocation of results or retries.
/// The caller owns stable inputs and output for the entire call, excludes
/// concurrent mutations and saves only at return boundaries.
class Simulation final {
  public:
    [[nodiscard]] static SimulationResult step(WorldState& world, RandomStreams& random,
                                               std::span<const ActionRequest> actions,
                                               std::span<ActionResult> results,
                                               SimulationLimits limits = {}) noexcept;
};

} // namespace corerpg
