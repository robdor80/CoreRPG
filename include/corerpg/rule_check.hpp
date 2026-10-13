#pragma once

#include <corerpg/entity_id.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace corerpg {

class WorldState;

/// Structural facts currently observable through WorldState.
/// Not a universal RPG predicate registry, policy language, or action dispatcher.
enum class RuleCheckKind : std::uint8_t {
    entity_exists,
    direct_container_is,
    entity_uncontained,
};

struct RuleCheck final {
    RuleCheckKind kind{RuleCheckKind::entity_exists};
    EntityId subject{};
    EntityId related{};  // required only for direct_container_is
};

enum class RuleCheckStatus : std::uint8_t {
    satisfied,
    unsatisfied,
    malformed_check,
    subject_not_found,
    related_not_found,
};

struct RuleCheckResult final {
    RuleCheckStatus status{RuleCheckStatus::malformed_check};
    [[nodiscard]] constexpr bool satisfied() const noexcept {
        return status == RuleCheckStatus::satisfied;
    }
};

/// Read-only deterministic evaluation. Never allocates, consumes RNG, or mutates WorldState.
/// A missing valid entity in an entity_exists check is an unsatisfied predicate,
/// whereas other checks distinguish missing subjects and missing related entities.
[[nodiscard]] RuleCheckResult evaluate_rule_check(
    const WorldState& world, const RuleCheck& check) noexcept;

struct RuleChecksResult final {
    RuleCheckStatus status{RuleCheckStatus::satisfied};
    std::size_t first_non_satisfied_index{0};

    [[nodiscard]] constexpr bool satisfied() const noexcept {
        return status == RuleCheckStatus::satisfied;
    }
};

/// Ordered all-of: stops at the first non-satisfied check. Empty is satisfied.
/// On success first_non_satisfied_index == checks.size().
[[nodiscard]] RuleChecksResult evaluate_rule_checks(
    const WorldState& world, std::span<const RuleCheck> checks) noexcept;

}  // namespace corerpg
