#pragma once

#include <corerpg/entity_structure.hpp>
#include <corerpg/rule_check.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

namespace corerpg {

class WorldState;
class RandomStreams;

struct PlaceEntityAction final {
    EntityId entity;
    EntityId container;
};
struct RemoveContainmentAction final {
    EntityId entity;
};
struct AddRelationAction final {
    EntityRelation relation;
};
struct RemoveRelationAction final {
    EntityRelation relation;
};
/// Rolls for the request's actor. Stream IDs are stable caller-assigned numbers.
struct StreamRollAction final {
    std::uint64_t stream_id{};
    std::uint64_t minimum{};
    std::uint64_t maximum{};
};

using ActionOperation = std::variant<PlaceEntityAction, RemoveContainmentAction,
                                     AddRelationAction, RemoveRelationAction, StreamRollAction>;

/// Privileged structured input, borrowed only for the duration of process/step.
/// Actor identifies the logical origin, not permission over the operation's targets.
struct ActionRequest final {
    EntityId actor{};
    ActionOperation operation{};
    std::optional<std::uint64_t> expected_revision{};
    std::span<const RuleCheck> preconditions{};
};

struct ActionLimits final {
    std::size_t max_preconditions{64};
};

enum class ActionOutcome : std::uint8_t { accepted, rejected, no_change };
enum class ActionRejection : std::uint8_t {
    none,
    precondition_limit_exceeded,
    actor_not_found,
    stale_revision,
    precondition_failed,
    entity_not_found,
    target_not_found,
    self_containment,
    cycle_detected,
    revision_exhausted,
    invalid_relation_kind,
    invalid_range,
    invalid_operation,
};

/// Invalid container denotes no direct containment, never a missing entity.
struct ContainmentConsequence final {
    EntityId entity;
    EntityId previous_container;
    EntityId container;
    bool operator==(const ContainmentConsequence&) const = default;
};
struct RelationConsequence final {
    EntityRelation relation;
    bool present{};
    bool operator==(const RelationConsequence&) const = default;
};
struct RollConsequence final {
    std::uint64_t stream_id{};
    std::uint64_t value{};
    bool consumed_random{};
    bool operator==(const RollConsequence&) const = default;
};
using ActionConsequence = std::variant<std::monostate, ContainmentConsequence,
                                       RelationConsequence, RollConsequence>;

/// Privileged internal result: may reveal hidden reality, references and revision.
/// Never hand it directly to an untrusted observer; use authorized ObserverView.
struct ActionResult final {
    ActionOutcome outcome{ActionOutcome::rejected};
    ActionRejection rejection{ActionRejection::invalid_operation};
    std::uint64_t revision_before{};
    std::uint64_t revision_after{};
    // Meaningful only for precondition_failed; index is the first failure.
    RuleCheckStatus precondition_status{RuleCheckStatus::satisfied};
    std::size_t precondition_index{};
    ActionConsequence consequence{};
    bool operator==(const ActionResult&) const = default;
};

/// Single authoritative action path. No callbacks, scripting or second world.
/// Rejection or allocation/capacity exception leaves world and RNG unchanged.
/// No-op/rejection never allocates or consumes RNG. The caller excludes concurrency
/// and authenticates/authorizes external requests before crossing this boundary.
class ActionEngine final {
  public:
    [[nodiscard]] static ActionResult process(WorldState& world, RandomStreams& random,
                                               const ActionRequest& request,
                                               ActionLimits limits = {});
};

} // namespace corerpg
