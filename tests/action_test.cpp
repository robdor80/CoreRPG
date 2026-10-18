#include <corerpg/action.hpp>
#include <corerpg/persistence.hpp>
#include <corerpg/roll_action.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace {
using namespace corerpg;
constexpr EntityId actor{1}, unit{2}, first{3}, second{4}, missing{99};
constexpr RelationKind kind{7};

class ActionTest : public ::testing::Test {
  protected:
    WorldState world;
    RandomStreams random{42};
    void SetUp() override {
        for (int i = 0; i < 4; ++i) ASSERT_TRUE(world.create_entity().is_valid());
    }
    ActionResult run(ActionOperation operation) {
        return ActionEngine::process(world, random, {actor, operation});
    }
    void rejected(const ActionRequest& request, ActionRejection reason) {
        const auto before = Persistence::capture(world, random);
        const auto result = ActionEngine::process(world, random, request);
        EXPECT_EQ(result.outcome, ActionOutcome::rejected);
        EXPECT_EQ(result.rejection, reason);
        EXPECT_TRUE(std::holds_alternative<std::monostate>(result.consequence));
        EXPECT_EQ(result.revision_before, before.revision);
        EXPECT_EQ(result.revision_after, before.revision);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
};

TEST_F(ActionTest, PlacementAndRemovalPublishExactlyOneTypedConsequence) {
    const auto revision = world.revision();
    const auto placed = run(PlaceEntityAction{unit, first});
    EXPECT_EQ(placed.outcome, ActionOutcome::accepted);
    EXPECT_EQ(placed.rejection, ActionRejection::none);
    EXPECT_EQ(placed.revision_before, revision);
    EXPECT_EQ(placed.revision_after, revision + 1);
    EXPECT_EQ(std::get<ContainmentConsequence>(placed.consequence),
              (ContainmentConsequence{unit, {}, first}));
    const auto moved = run(PlaceEntityAction{unit, second});
    EXPECT_EQ(std::get<ContainmentConsequence>(moved.consequence),
              (ContainmentConsequence{unit, first, second}));
    const auto removed = run(RemoveContainmentAction{unit});
    EXPECT_EQ(removed.revision_after, revision + 3);
    EXPECT_EQ(std::get<ContainmentConsequence>(removed.consequence),
              (ContainmentConsequence{unit, second, {}}));
    EXPECT_EQ(world.container_of(unit).status, ContainmentStatus::uncontained);
    EXPECT_EQ(random.stream_count(), 0U);
}

TEST_F(ActionTest, RepeatedStructuralOperationsAreNoOpsWithoutConsequences) {
    ASSERT_EQ(run(PlaceEntityAction{unit, first}).outcome, ActionOutcome::accepted);
    const auto before = Persistence::capture(world, random);
    auto result = run(PlaceEntityAction{unit, first});
    EXPECT_EQ(result.outcome, ActionOutcome::no_change);
    EXPECT_EQ(result.rejection, ActionRejection::none);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(result.consequence));
    EXPECT_EQ(Persistence::capture(world, random), before);
    ASSERT_EQ(run(RemoveContainmentAction{unit}).outcome, ActionOutcome::accepted);
    result = run(RemoveContainmentAction{unit});
    EXPECT_EQ(result.outcome, ActionOutcome::no_change);
    EXPECT_EQ(result.revision_before, result.revision_after);
}

TEST_F(ActionTest, ActorIsLogicalOriginAndMayDifferFromOperationSubject) {
    EXPECT_EQ(run(PlaceEntityAction{unit, first}).outcome, ActionOutcome::accepted);
    EXPECT_EQ(world.container_of(actor).status, ContainmentStatus::uncontained);
    rejected({{}, RemoveContainmentAction{unit}}, ActionRejection::actor_not_found);
    rejected({missing, RemoveContainmentAction{unit}}, ActionRejection::actor_not_found);
}

TEST_F(ActionTest, InvalidContainmentReferencesSelfAndCyclesAreRejectedAtomically) {
    rejected({actor, PlaceEntityAction{{}, first}}, ActionRejection::entity_not_found);
    rejected({actor, RemoveContainmentAction{missing}}, ActionRejection::entity_not_found);
    rejected({actor, PlaceEntityAction{unit, missing}}, ActionRejection::target_not_found);
    rejected({actor, PlaceEntityAction{unit, {}}}, ActionRejection::target_not_found);
    rejected({actor, PlaceEntityAction{unit, unit}}, ActionRejection::self_containment);
    ASSERT_EQ(run(PlaceEntityAction{unit, first}).outcome, ActionOutcome::accepted);
    ASSERT_EQ(run(PlaceEntityAction{first, second}).outcome, ActionOutcome::accepted);
    rejected({actor, PlaceEntityAction{second, unit}}, ActionRejection::cycle_detected);
}

TEST_F(ActionTest, RelationChangesUseExactTriplesWithoutSocialSemantics) {
    const EntityRelation relation{unit, kind, first};
    const auto added = run(AddRelationAction{relation});
    EXPECT_EQ(added.outcome, ActionOutcome::accepted);
    EXPECT_EQ(std::get<RelationConsequence>(added.consequence),
              (RelationConsequence{relation, true}));
    EXPECT_TRUE(world.has_relation(relation));
    EXPECT_FALSE(world.has_relation({first, kind, unit}));
    EXPECT_EQ(run(AddRelationAction{relation}).outcome, ActionOutcome::no_change);
    const auto removed = run(RemoveRelationAction{relation});
    EXPECT_EQ(removed.outcome, ActionOutcome::accepted);
    EXPECT_EQ(std::get<RelationConsequence>(removed.consequence),
              (RelationConsequence{relation, false}));
    EXPECT_EQ(run(RemoveRelationAction{relation}).outcome, ActionOutcome::no_change);
    EXPECT_EQ(run(AddRelationAction{{unit, kind, unit}}).outcome, ActionOutcome::accepted);
}

TEST_F(ActionTest, RelationsRejectMissingEndpointsAndZeroKind) {
    rejected({actor, AddRelationAction{{missing, kind, unit}}}, ActionRejection::entity_not_found);
    rejected({actor, AddRelationAction{{unit, kind, missing}}}, ActionRejection::target_not_found);
    rejected({actor, AddRelationAction{{unit, {}, first}}}, ActionRejection::invalid_relation_kind);
    rejected({actor, RemoveRelationAction{{missing, kind, unit}}}, ActionRejection::entity_not_found);
    rejected({actor, RemoveRelationAction{{unit, kind, missing}}}, ActionRejection::target_not_found);
    rejected({actor, RemoveRelationAction{{unit, {}, first}}}, ActionRejection::invalid_relation_kind);
}

TEST_F(ActionTest, StaleRevisionIsCheckedBeforePreconditionsOrRng) {
    const auto revision = world.revision();
    const std::array invalid{RuleCheck{static_cast<RuleCheckKind>(255), unit}};
    rejected({actor, StreamRollAction{9, 1, 100}, revision - 1, invalid},
             ActionRejection::stale_revision);
    const ActionRequest guarded{actor, PlaceEntityAction{unit, first}, revision};
    EXPECT_EQ(ActionEngine::process(world, random, guarded).outcome, ActionOutcome::accepted);
    rejected(guarded, ActionRejection::stale_revision);
}

TEST_F(ActionTest, PreconditionsAreOrderedAndEvaluatedAgainstCurrentReality) {
    const std::array checks{RuleCheck{RuleCheckKind::entity_exists, unit},
                            RuleCheck{RuleCheckKind::direct_container_is, unit, first},
                            RuleCheck{RuleCheckKind::entity_exists, missing}};
    const ActionRequest request{actor, PlaceEntityAction{unit, second}, {}, checks};
    auto result = ActionEngine::process(world, random, request);
    EXPECT_EQ(result.rejection, ActionRejection::precondition_failed);
    EXPECT_EQ(result.precondition_index, 1U);
    EXPECT_EQ(result.precondition_status, RuleCheckStatus::unsatisfied);
    ASSERT_EQ(run(PlaceEntityAction{unit, first}).outcome, ActionOutcome::accepted);
    result = ActionEngine::process(world, random, request);
    EXPECT_EQ(result.precondition_index, 2U);
    const ActionRequest shorter{actor, PlaceEntityAction{unit, second}, {},
                                std::span{checks}.first(2)};
    EXPECT_EQ(ActionEngine::process(world, random, shorter).outcome, ActionOutcome::accepted);
    rejected(shorter, ActionRejection::precondition_failed);
}

TEST_F(ActionTest, MalformedAndMissingPreconditionsRemainDiagnosableWithoutEffects) {
    const std::array checks{
        RuleCheck{static_cast<RuleCheckKind>(255), unit},
        RuleCheck{RuleCheckKind::entity_exists, {}, {}},
        RuleCheck{RuleCheckKind::direct_container_is, missing, first},
        RuleCheck{RuleCheckKind::direct_container_is, unit, missing}};
    const std::array statuses{RuleCheckStatus::malformed_check, RuleCheckStatus::malformed_check,
                              RuleCheckStatus::subject_not_found, RuleCheckStatus::related_not_found};
    for (std::size_t i = 0; i < checks.size(); ++i) {
        const auto before = Persistence::capture(world, random);
        const auto result = ActionEngine::process(world, random,
            {actor, StreamRollAction{9, 0, 100}, {}, std::span{checks}.subspan(i, 1)});
        EXPECT_EQ(result.rejection, ActionRejection::precondition_failed);
        EXPECT_EQ(result.precondition_status, statuses[i]);
        EXPECT_EQ(Persistence::capture(world, random), before);
    }
}

TEST_F(ActionTest, PreconditionBudgetIsCheckedBeforeExaminingInputFacts) {
    const std::array checks{RuleCheck{RuleCheckKind::entity_exists, unit}};
    const auto before = Persistence::capture(world, random);
    const auto result = ActionEngine::process(world, random,
        {{}, StreamRollAction{9, 1, 9}, {}, checks}, {0});
    EXPECT_EQ(result.rejection, ActionRejection::precondition_limit_exceeded);
    EXPECT_EQ(Persistence::capture(world, random), before);
    EXPECT_EQ(ActionEngine::process(world, random, {actor, StreamRollAction{9, 5, 5}}, {0}).outcome,
              ActionOutcome::accepted);
}

TEST_F(ActionTest, StreamRollMatchesLegacyPrimitiveAndNeverMutatesWorld) {
    RandomStreams expected{42};
    const auto revision = world.revision();
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 3> ranges{{
        {0, maximum}, {0, (std::uint64_t{1} << 63) + 1}, {3, 900}}};
    for (const auto& [minimum, max] : ranges) {
        const auto result = run(StreamRollAction{9, minimum, max});
        EXPECT_EQ(result.outcome, ActionOutcome::accepted);
        EXPECT_EQ(result.revision_after, revision);
        const auto roll = resolve_roll(world, expected.stream(9), {actor, minimum, max});
        EXPECT_EQ(std::get<RollConsequence>(result.consequence),
                  (RollConsequence{9, roll.value, true}));
        EXPECT_EQ(random.find_stream(9)->state(), expected.find_stream(9)->state());
    }
}

TEST_F(ActionTest, InvalidAndSingletonRollsDoNotOpenOrAdvanceStreams) {
    rejected({actor, StreamRollAction{9, 10, 1}}, ActionRejection::invalid_range);
    rejected({missing, StreamRollAction{9, 1, 10}}, ActionRejection::actor_not_found);
    const auto before = Persistence::capture(world, random);
    const auto result = run(StreamRollAction{9, 5, 5});
    EXPECT_EQ(result.outcome, ActionOutcome::accepted);
    EXPECT_EQ(std::get<RollConsequence>(result.consequence), (RollConsequence{9, 5, false}));
    EXPECT_EQ(Persistence::capture(world, random), before);
    static_cast<void>(random.stream(9).next());
    const auto opened = Persistence::capture(world, random);
    EXPECT_EQ(run(StreamRollAction{9, 1, 1}).outcome, ActionOutcome::accepted);
    EXPECT_EQ(Persistence::capture(world, random), opened);
}

TEST_F(ActionTest, RevisionExhaustionRejectsMutationsButAllowsNoOpsAndRolls) {
    ASSERT_EQ(run(PlaceEntityAction{unit, first}).outcome, ActionOutcome::accepted);
    ASSERT_EQ(run(AddRelationAction{{unit, kind, first}}).outcome, ActionOutcome::accepted);
    auto snapshot = Persistence::capture(world, random);
    snapshot.revision = std::numeric_limits<std::uint64_t>::max();
    ASSERT_EQ(Persistence::restore(snapshot, world, random), PersistenceStatus::success);
    rejected({actor, PlaceEntityAction{unit, second}}, ActionRejection::revision_exhausted);
    rejected({actor, RemoveContainmentAction{unit}}, ActionRejection::revision_exhausted);
    rejected({actor, AddRelationAction{{unit, kind, second}}}, ActionRejection::revision_exhausted);
    rejected({actor, RemoveRelationAction{{unit, kind, first}}}, ActionRejection::revision_exhausted);
    EXPECT_EQ(run(PlaceEntityAction{unit, first}).outcome, ActionOutcome::no_change);
    EXPECT_EQ(run(AddRelationAction{{unit, kind, first}}).outcome, ActionOutcome::no_change);
    EXPECT_EQ(run(RemoveContainmentAction{actor}).outcome, ActionOutcome::no_change);
    EXPECT_EQ(run(RemoveRelationAction{{unit, kind, second}}).outcome, ActionOutcome::no_change);
    EXPECT_EQ(run(StreamRollAction{9, 0, 10}).outcome, ActionOutcome::accepted);
}

} // namespace
