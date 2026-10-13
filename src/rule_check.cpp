#include <corerpg/rule_check.hpp>

#include <corerpg/world_state.hpp>

namespace corerpg {

RuleCheckResult evaluate_rule_check(const WorldState& world, const RuleCheck& check) noexcept {
    if (!check.subject.is_valid()) {
        return {RuleCheckStatus::malformed_check};
    }

    switch (check.kind) {
    case RuleCheckKind::entity_exists:
        if (check.related.is_valid()) {
            return {RuleCheckStatus::malformed_check};
        }
        return {world.contains(check.subject) ? RuleCheckStatus::satisfied
                                              : RuleCheckStatus::unsatisfied};

    case RuleCheckKind::direct_container_is: {
        if (!check.related.is_valid()) {
            return {RuleCheckStatus::malformed_check};
        }
        if (!world.contains(check.subject)) {
            return {RuleCheckStatus::subject_not_found};
        }
        if (!world.contains(check.related)) {
            return {RuleCheckStatus::related_not_found};
        }
        const auto placement = world.container_of(check.subject);
        return {placement.status == ContainmentStatus::contained &&
                        placement.container == check.related
                    ? RuleCheckStatus::satisfied
                    : RuleCheckStatus::unsatisfied};
    }

    case RuleCheckKind::entity_uncontained: {
        if (check.related.is_valid()) {
            return {RuleCheckStatus::malformed_check};
        }
        if (!world.contains(check.subject)) {
            return {RuleCheckStatus::subject_not_found};
        }
        const auto placement = world.container_of(check.subject);
        return {placement.status == ContainmentStatus::uncontained
                    ? RuleCheckStatus::satisfied
                    : RuleCheckStatus::unsatisfied};
    }
    }

    return {RuleCheckStatus::malformed_check};
}

RuleChecksResult evaluate_rule_checks(
    const WorldState& world, std::span<const RuleCheck> checks) noexcept {
    for (std::size_t index = 0; index < checks.size(); ++index) {
        const auto result = evaluate_rule_check(world, checks[index]);
        if (!result.satisfied()) {
            return {result.status, index};
        }
    }
    return {RuleCheckStatus::satisfied, checks.size()};
}

}  // namespace corerpg
