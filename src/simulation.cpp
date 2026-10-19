#include <corerpg/simulation.hpp>

#include <new>
#include <stdexcept>
#include <type_traits>

namespace corerpg {

SimulationResult Simulation::step(WorldState& world, RandomStreams& random,
                                  std::span<const ActionRequest> actions,
                                  std::span<ActionResult> results,
                                  SimulationLimits limits) noexcept {
    static_assert(std::is_nothrow_copy_assignable_v<ActionResult>);
    if (actions.size() > limits.max_actions) {
        return {SimulationStatus::action_limit_exceeded, 0};
    }
    if (results.size() < actions.size()) {
        return {SimulationStatus::insufficient_output, 0};
    }
    std::size_t processed = 0;
    try {
        for (const auto& action : actions) {
            results[processed] = ActionEngine::process(world, random, action, limits.action_limits);
            ++processed;
        }
    } catch (const std::bad_alloc&) {
        return {SimulationStatus::allocation_failure, processed};
    } catch (const std::length_error&) {
        return {SimulationStatus::capacity_failure, processed};
    }
    return {SimulationStatus::completed, processed};
}

} // namespace corerpg
