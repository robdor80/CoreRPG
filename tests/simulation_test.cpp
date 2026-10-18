#include <corerpg/persistence.hpp>
#include <corerpg/simulation.hpp>

#include <gtest/gtest.h>

#include <array>

namespace {
using namespace corerpg;
constexpr EntityId actor{1}, unit{2}, first{3}, second{4};

class SimulationTest : public ::testing::Test {
  protected:
    WorldState world;
    RandomStreams random{42};
    void SetUp() override {
        for (int i = 0; i < 4; ++i) ASSERT_TRUE(world.create_entity().is_valid());
    }
};

TEST_F(SimulationTest, ActionsObserveEarlierConsequencesInExplicitInputOrder) {
    const std::array checks{RuleCheck{RuleCheckKind::direct_container_is, unit, first}};
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, first}},
        ActionRequest{actor, PlaceEntityAction{unit, second}, {}, checks},
        ActionRequest{actor, PlaceEntityAction{unit, first}, {}, checks},
        ActionRequest{unit, AddRelationAction{{unit, RelationKind{7}, actor}}}};
    std::array<ActionResult, actions.size()> output;
    const auto revision = world.revision();
    EXPECT_EQ(Simulation::step(world, random, actions, output),
              (SimulationResult{SimulationStatus::completed, 4}));
    EXPECT_EQ(output[0].outcome, ActionOutcome::accepted);
    EXPECT_EQ(output[1].outcome, ActionOutcome::accepted);
    EXPECT_EQ(output[2].rejection, ActionRejection::precondition_failed);
    EXPECT_EQ(output[3].outcome, ActionOutcome::accepted);
    EXPECT_EQ(output[2].revision_before, revision + 2);
    EXPECT_EQ(output[3].revision_after, revision + 3);
    EXPECT_EQ(world.container_of(unit).container, second);
}

TEST_F(SimulationTest, SameRevisionRequestsConflictAfterFirstEffectiveChange) {
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, first}, world.revision()},
        ActionRequest{actor, PlaceEntityAction{unit, second}, world.revision()},
        ActionRequest{actor, StreamRollAction{9, 0, 100}, world.revision()}};
    std::array<ActionResult, actions.size()> output;
    EXPECT_EQ(Simulation::step(world, random, actions, output).processed, 3U);
    EXPECT_EQ(output[0].outcome, ActionOutcome::accepted);
    EXPECT_EQ(output[1].rejection, ActionRejection::stale_revision);
    EXPECT_EQ(output[2].rejection, ActionRejection::stale_revision);
    EXPECT_EQ(random.stream_count(), 0U);
}

TEST_F(SimulationTest, NoOpDoesNotInvalidateFollowingRevisionGuard) {
    ASSERT_EQ(world.place_entity(unit, first), ContainmentChange::applied);
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, first}, world.revision()},
        ActionRequest{actor, PlaceEntityAction{unit, second}, world.revision()}};
    std::array<ActionResult, actions.size()> output;
    EXPECT_EQ(Simulation::step(world, random, actions, output).processed, 2U);
    EXPECT_EQ(output[0].outcome, ActionOutcome::no_change);
    EXPECT_EQ(output[1].outcome, ActionOutcome::accepted);
}

TEST_F(SimulationTest, OversizedBatchAndShortOutputRejectWholeStepBeforeAnyAction) {
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, first}},
        ActionRequest{actor, StreamRollAction{9, 0, 100}}};
    std::array<ActionResult, 3> output;
    output[0].revision_before = 999;
    const auto untouched = output;
    const auto before = Persistence::capture(world, random);
    EXPECT_EQ(Simulation::step(world, random, actions, output, {1}),
              (SimulationResult{SimulationStatus::action_limit_exceeded, 0}));
    EXPECT_EQ(output, untouched);
    EXPECT_EQ(Simulation::step(world, random, actions, std::span{output}.first(1)),
              (SimulationResult{SimulationStatus::insufficient_output, 0}));
    EXPECT_EQ(output, untouched);
    EXPECT_EQ(Persistence::capture(world, random), before);
    EXPECT_EQ(Simulation::step(world, random, actions, output, {2}).processed, 2U);
    EXPECT_EQ(output[2], untouched[2]);
}

TEST_F(SimulationTest, EmptyAndZeroBudgetStepsDoNotInventTimeOrMutateState) {
    const auto before = Persistence::capture(world, random);
    EXPECT_EQ(Simulation::step(world, random, {}, {}, {0}),
              (SimulationResult{SimulationStatus::completed, 0}));
    EXPECT_EQ(Persistence::capture(world, random), before);
    const std::array actions{ActionRequest{actor, RemoveContainmentAction{unit}}};
    std::array<ActionResult, 1> output;
    EXPECT_EQ(Simulation::step(world, random, actions, output, {0}).status,
              SimulationStatus::action_limit_exceeded);
}

TEST_F(SimulationTest, PerActionBudgetRejectsOneRequestAndContinuesTheBatch) {
    const std::array checks{RuleCheck{RuleCheckKind::entity_exists, unit}};
    const std::array actions{
        ActionRequest{actor, StreamRollAction{9, 0, 100}, {}, checks},
        ActionRequest{actor, PlaceEntityAction{unit, first}}};
    std::array<ActionResult, 2> output;
    EXPECT_EQ(Simulation::step(world, random, actions, output, {2, {0}}).processed, 2U);
    EXPECT_EQ(output[0].rejection, ActionRejection::precondition_limit_exceeded);
    EXPECT_EQ(output[1].outcome, ActionOutcome::accepted);
    EXPECT_EQ(random.stream_count(), 0U);
}

TEST_F(SimulationTest, BatchPartitionDoesNotAlterCausalityOrRng) {
    WorldState other{world};
    RandomStreams other_random{random};
    const std::array actions{
        ActionRequest{actor, StreamRollAction{9, 0, 100}},
        ActionRequest{actor, PlaceEntityAction{unit, first}},
        ActionRequest{actor, StreamRollAction{9, 0, 100}},
        ActionRequest{actor, PlaceEntityAction{unit, second}},
        ActionRequest{actor, StreamRollAction{0, 0, 100}}};
    std::array<ActionResult, actions.size()> together, split;
    EXPECT_EQ(Simulation::step(world, random, actions, together).processed, actions.size());
    EXPECT_EQ(Simulation::step(other, other_random, std::span{actions}.first(2),
                              std::span{split}.first(2)).processed, 2U);
    EXPECT_EQ(Simulation::step(other, other_random, std::span{actions}.subspan(2),
                              std::span{split}.subspan(2)).processed, 3U);
    EXPECT_EQ(together, split);
    EXPECT_EQ(Persistence::capture(world, random), Persistence::capture(other, other_random));
}

TEST_F(SimulationTest, ExplicitRepeatsAreNoOpsForLinksAndNewDrawsForRolls) {
    const std::array actions{
        ActionRequest{actor, AddRelationAction{{unit, RelationKind{7}, first}}},
        ActionRequest{actor, StreamRollAction{9, 0, 100}}};
    std::array<ActionResult, 2> initial, repeated;
    ASSERT_EQ(Simulation::step(world, random, actions, initial).processed, 2U);
    const auto state = random.find_stream(9)->state();
    EXPECT_EQ(Simulation::step(world, random, actions, repeated).processed, 2U);
    EXPECT_EQ(repeated[0].outcome, ActionOutcome::no_change);
    EXPECT_EQ(repeated[1].outcome, ActionOutcome::accepted);
    EXPECT_NE(random.find_stream(9)->state(), state);
}

TEST_F(SimulationTest, LogicalCohortContinuesWithNoHostRepresentations) {
    // Neutral need shared by an unloaded 3D crew compartment and logical units.
    ASSERT_EQ(world.declare_population(actor, 300), PopulationChange::applied);
    const auto batch = world.materialize_population(actor, 12);
    ASSERT_EQ(batch.status, MaterializationStatus::applied);
    std::array<ActionRequest, 12> actions;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const EntityId individual{batch.first.value() + i};
        actions[i] = {individual, PlaceEntityAction{individual, first}};
    }
    std::array<ActionResult, actions.size()> output;
    EXPECT_EQ(Simulation::step(world, random, actions, output).processed, actions.size());
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const EntityId individual{batch.first.value() + i};
        EXPECT_EQ(output[i].outcome, ActionOutcome::accepted);
        EXPECT_EQ(world.container_of(individual).container, first);
        EXPECT_EQ(world.population_origin(individual), actor);
    }
    EXPECT_EQ(world.population_of(actor)->latent, 288U);
    EXPECT_EQ(world.entity_count(), 16U);
}

TEST_F(SimulationTest, DistantInhabitantsKeepIdentityAndConsequencesAcrossSteps) {
    // Neutral need shared by an unloaded 2D settlement and nonhuman aggregates.
    ASSERT_EQ(world.declare_population(first, 1000), PopulationChange::applied);
    const auto batch = world.materialize_population(first, 2);
    ASSERT_EQ(batch.status, MaterializationStatus::applied);
    const EntityId visitor = batch.first;
    const EntityId peer{visitor.value() + 1};
    const std::array outward{
        ActionRequest{visitor, PlaceEntityAction{visitor, second}},
        ActionRequest{peer, AddRelationAction{{peer, RelationKind{17}, visitor}}}};
    std::array<ActionResult, 2> output;
    EXPECT_EQ(Simulation::step(world, random, outward, output).processed, 2U);
    const std::array checks{RuleCheck{RuleCheckKind::direct_container_is, visitor, second}};
    const std::array onward{
        ActionRequest{peer, PlaceEntityAction{peer, second}, {}, checks}};
    EXPECT_EQ(Simulation::step(world, random, onward, output).processed, 1U);
    EXPECT_EQ(world.container_of(peer).container, second);
    EXPECT_TRUE(world.has_relation({peer, RelationKind{17}, visitor}));
    EXPECT_EQ(world.population_origin(visitor), first);
    EXPECT_EQ(world.population_of(first)->latent, 998U);
    EXPECT_EQ(world.entity_count(), 6U);
}

} // namespace
