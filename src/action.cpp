#include <corerpg/action.hpp>

#include <corerpg/random_streams.hpp>
#include <corerpg/roll_action.hpp>
#include <corerpg/world_state.hpp>

#include <type_traits>

namespace corerpg {
namespace {

ActionRejection rejection_of(ContainmentChange change) noexcept {
    switch (change) {
    case ContainmentChange::applied:
    case ContainmentChange::no_change:
        return ActionRejection::none;
    case ContainmentChange::entity_not_found:
        return ActionRejection::entity_not_found;
    case ContainmentChange::container_not_found:
        return ActionRejection::target_not_found;
    case ContainmentChange::self_containment:
        return ActionRejection::self_containment;
    case ContainmentChange::cycle_detected:
        return ActionRejection::cycle_detected;
    case ContainmentChange::revision_exhausted:
        return ActionRejection::revision_exhausted;
    }
    return ActionRejection::invalid_operation;
}

ActionRejection rejection_of(RelationChange change) noexcept {
    switch (change) {
    case RelationChange::applied:
    case RelationChange::no_change:
        return ActionRejection::none;
    case RelationChange::source_not_found:
        return ActionRejection::entity_not_found;
    case RelationChange::target_not_found:
        return ActionRejection::target_not_found;
    case RelationChange::invalid_kind:
        return ActionRejection::invalid_relation_kind;
    case RelationChange::revision_exhausted:
        return ActionRejection::revision_exhausted;
    }
    return ActionRejection::invalid_operation;
}

void finish_containment(ActionResult& result, ContainmentChange change, EntityId entity,
                        EntityId previous, EntityId container) noexcept {
    result.rejection = rejection_of(change);
    if (change == ContainmentChange::applied) {
        result.outcome = ActionOutcome::accepted;
        result.consequence = ContainmentConsequence{entity, previous, container};
    } else if (change == ContainmentChange::no_change) {
        result.outcome = ActionOutcome::no_change;
    }
}

void finish_relation(ActionResult& result, RelationChange change, EntityRelation relation,
                     bool present) noexcept {
    result.rejection = rejection_of(change);
    if (change == RelationChange::applied) {
        result.outcome = ActionOutcome::accepted;
        result.consequence = RelationConsequence{relation, present};
    } else if (change == RelationChange::no_change) {
        result.outcome = ActionOutcome::no_change;
    }
}

} // namespace

ActionResult ActionEngine::process(WorldState& world, RandomStreams& random,
                                   const ActionRequest& request, ActionLimits limits) {
    // No result allocation, conversion or move can fail after a primitive publishes.
    static_assert(std::is_nothrow_copy_assignable_v<ActionResult>);
    static_assert(std::is_nothrow_move_constructible_v<ActionResult>);
    static_assert(std::is_nothrow_constructible_v<ActionConsequence, ContainmentConsequence>);
    static_assert(std::is_nothrow_constructible_v<ActionConsequence, RelationConsequence>);
    static_assert(std::is_nothrow_constructible_v<ActionConsequence, RollConsequence>);
    ActionResult result;
    result.revision_before = result.revision_after = world.revision();
    if (request.preconditions.size() > limits.max_preconditions) {
        result.rejection = ActionRejection::precondition_limit_exceeded;
        return result;
    }
    if (!world.contains(request.actor)) {
        result.rejection = ActionRejection::actor_not_found;
        return result;
    }
    if (request.expected_revision && *request.expected_revision != world.revision()) {
        result.rejection = ActionRejection::stale_revision;
        return result;
    }
    const auto checks = evaluate_rule_checks(world, request.preconditions);
    if (!checks.satisfied()) {
        result.rejection = ActionRejection::precondition_failed;
        result.precondition_status = checks.status;
        result.precondition_index = checks.first_non_satisfied_index;
        return result;
    }
    if (request.operation.valueless_by_exception()) {
        return result;
    }
    std::visit(
        [&](const auto& operation) {
            using Operation = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<Operation, PlaceEntityAction>) {
                const auto previous = world.container_of(operation.entity).container;
                const auto change = world.place_entity(operation.entity, operation.container);
                finish_containment(result, change, operation.entity, previous, operation.container);
            } else if constexpr (std::is_same_v<Operation, RemoveContainmentAction>) {
                const auto previous = world.container_of(operation.entity).container;
                const auto change = world.remove_containment(operation.entity);
                finish_containment(result, change, operation.entity, previous, {});
            } else if constexpr (std::is_same_v<Operation, AddRelationAction>) {
                finish_relation(result, world.add_relation(operation.relation), operation.relation,
                                true);
            } else if constexpr (std::is_same_v<Operation, RemoveRelationAction>) {
                finish_relation(result, world.remove_relation(operation.relation),
                                operation.relation, false);
            } else if constexpr (std::is_same_v<Operation, StreamRollAction>) {
                if (operation.minimum > operation.maximum) {
                    result.rejection = ActionRejection::invalid_range;
                    return;
                }
                const RollAction roll{request.actor, operation.minimum, operation.maximum};
                // Valid singleton ranges do not open streams. Resolve through the legacy
                // primitive as well, preserving its range and SplitMix64 contracts.
                DeterministicRandom unused{0};
                const bool consumes = operation.minimum != operation.maximum;
                auto& stream = consumes ? random.stream(operation.stream_id) : unused;
                // stream insertion is the last throwing operation; resolve_roll is noexcept.
                const auto rolled = resolve_roll(world, stream, roll);
                result.outcome = ActionOutcome::accepted;
                result.rejection = ActionRejection::none;
                result.consequence = RollConsequence{operation.stream_id, rolled.value, consumes};
            }
        },
        request.operation);
    result.revision_after = world.revision();
    return result;
}

} // namespace corerpg
