#include <corerpg/persistence.hpp>
#include <corerpg/simulation.hpp>
#include <corerpg/visibility.hpp>

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace {
using namespace corerpg;
constexpr EntityId actor{1}, unit{2}, first{3}, second{4}, informed{5}, detected{6}, hidden{7};
constexpr RelationKind kind{17};

class ActionPersistenceTest : public ::testing::Test {
  protected:
    WorldState world;
    RandomStreams random{123};
    void SetUp() override {
        for (int i = 0; i < 7; ++i) ASSERT_TRUE(world.create_entity().is_valid());
        ASSERT_EQ(world.place_entity(unit, first), ContainmentChange::applied);
        for (const auto subject : {unit, first, second}) {
            ASSERT_EQ(world.learn_fact(informed, {subject, FactKind::existence, {}, {}}), KnowledgeChange::applied);
        }
        ASSERT_EQ(world.learn_fact(informed, {unit, FactKind::containment, {}, {}}), KnowledgeChange::applied);
        ASSERT_EQ(world.learn_fact(informed, {unit, FactKind::relation, kind, second}),
                  KnowledgeChange::applied); // known absence
        ASSERT_EQ(world.learn_fact(detected, {unit, FactKind::existence, {}, {}}), KnowledgeChange::applied);
    }
};

TEST_F(ActionPersistenceTest, InternalConsequencesNeverRefreshMemoriesOrGrantHiddenFacts) {
    const auto informed_before = Visibility::project(world, informed);
    const auto detected_before = Visibility::project(world, detected);
    const auto knowledge_before = Persistence::capture(world, random).knowledge;
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, second}},
        ActionRequest{unit, AddRelationAction{{unit, kind, second}}},
        ActionRequest{unit, AddRelationAction{{unit, kind, hidden}}},
        ActionRequest{hidden, StreamRollAction{0, 0, 100}}};
    std::array<ActionResult, actions.size()> output;
    ASSERT_EQ(Simulation::step(world, random, actions, output).processed, actions.size());
    for (const auto& result : output) EXPECT_EQ(result.outcome, ActionOutcome::accepted);
    EXPECT_EQ(Persistence::capture(world, random).knowledge, knowledge_before);
    EXPECT_EQ(Visibility::project(world, informed), informed_before);
    EXPECT_EQ(Visibility::project(world, detected), detected_before);
    EXPECT_EQ(informed_before.find({unit, FactKind::containment, {}, {}})->reference, first);
    EXPECT_FALSE(informed_before.find({unit, FactKind::relation, kind, second})->present);
    EXPECT_EQ(detected_before.find({unit, FactKind::containment, {}, {}}), nullptr);
    EXPECT_FALSE(Visibility::project(world, informed).knows(hidden));
    EXPECT_EQ(Visibility::project(world, detected).find({unit, FactKind::relation, kind, hidden}), nullptr);
    const std::array grants{FactKey{unit, FactKind::relation, kind, hidden}};
    EXPECT_EQ(Visibility::project(world, informed, grants), informed_before);
    // A separate explicit observation can update only one observer's memory.
    ASSERT_EQ(world.learn_fact(informed, {unit, FactKind::containment, {}, {}}), KnowledgeChange::applied);
    EXPECT_EQ(Visibility::project(world, informed).find({unit, FactKind::containment, {}, {}})->reference, second);
    EXPECT_EQ(Visibility::project(world, detected), detected_before);
}

TEST_F(ActionPersistenceTest, SavingAndRestoringConservesConsequencesIndependentMemoriesAndRng) {
    ASSERT_EQ(world.declare_population(first, 1000), PopulationChange::applied);
    const auto individuals = world.materialize_population(first, 3);
    ASSERT_EQ(individuals.status, MaterializationStatus::applied);
    static_cast<void>(random.stream(0).next());
    static_cast<void>(random.stream(std::numeric_limits<std::uint64_t>::max()).next());
    const auto informed_before = Visibility::project(world, informed);
    const auto detected_before = Visibility::project(world, detected);
    const auto initial = Persistence::capture(world, random);
    ASSERT_EQ(initial.format_version, save_format_version_v3);
    const auto saved = Persistence::serialize(initial);
    ASSERT_EQ(saved.status, PersistenceStatus::success);
    WorldState restored;
    RandomStreams restored_random{999};
    ASSERT_EQ(Persistence::load(saved.bytes, restored, restored_random), PersistenceStatus::success);

    for (std::uint64_t step = 0; step < 100; ++step) {
        const auto old_container = (step % 2 == 0) ? first : second;
        const auto new_container = (step % 2 == 0) ? second : first;
        const std::array checks{RuleCheck{RuleCheckKind::direct_container_is, unit, old_container}};
        const ActionOperation relation = (step % 2 == 0)
            ? ActionOperation{AddRelationAction{{unit, kind, second}}}
            : ActionOperation{RemoveRelationAction{{unit, kind, second}}};
        const std::array actions{
            ActionRequest{actor, PlaceEntityAction{unit, new_container}, world.revision(), checks},
            ActionRequest{unit, relation},
            ActionRequest{unit, StreamRollAction{0, 0, (std::uint64_t{1} << 63) + 1}},
            ActionRequest{individuals.first, StreamRollAction{step + 17, 1, 100}},
            ActionRequest{hidden, StreamRollAction{std::numeric_limits<std::uint64_t>::max(), 0,
                                                 std::numeric_limits<std::uint64_t>::max()}}};
        std::array<ActionResult, actions.size()> original_results, restored_results;
        ASSERT_EQ(Simulation::step(world, random, actions, original_results).processed, actions.size());
        ASSERT_EQ(Simulation::step(restored, restored_random, actions, restored_results).processed,
                  actions.size());
        EXPECT_EQ(original_results, restored_results) << step;
        for (const auto& result : original_results) EXPECT_EQ(result.outcome, ActionOutcome::accepted);
        const auto expected = Persistence::capture(world, random);
        const auto actual = Persistence::capture(restored, restored_random);
        EXPECT_EQ(actual, expected) << step;
        EXPECT_EQ(actual.knowledge, initial.knowledge);
        const auto encoded = Persistence::serialize(expected);
        EXPECT_EQ(Persistence::serialize(actual).bytes, encoded.bytes) << step;
        EXPECT_EQ(Visibility::project(restored, informed), informed_before);
        EXPECT_EQ(Visibility::project(restored, detected), detected_before);
        EXPECT_EQ(restored.population_of(first)->latent, 997U);
        EXPECT_EQ(restored.population_origin(individuals.first), first);
        if (step % 9 == 0) {
            ASSERT_EQ(Persistence::load(encoded.bytes, restored, restored_random), PersistenceStatus::success);
        }
    }
}

TEST_F(ActionPersistenceTest, RejectedAndNoOpRequestsLeaveCanonicalSaveBytesUnchanged) {
    const auto before = Persistence::serialize(Persistence::capture(world, random));
    const std::array checks{RuleCheck{RuleCheckKind::entity_uncontained, unit}};
    const std::array actions{
        ActionRequest{actor, PlaceEntityAction{unit, first}},
        ActionRequest{actor, StreamRollAction{0, 0, 100}, {}, checks},
        ActionRequest{actor, PlaceEntityAction{first, unit}},
        ActionRequest{actor, RemoveRelationAction{{unit, kind, first}}},
        ActionRequest{actor, StreamRollAction{0, 9, 0}}};
    std::array<ActionResult, actions.size()> output;
    ASSERT_EQ(Simulation::step(world, random, actions, output).processed, actions.size());
    EXPECT_EQ(output[0].outcome, ActionOutcome::no_change);
    EXPECT_EQ(output[1].rejection, ActionRejection::precondition_failed);
    EXPECT_EQ(output[2].rejection, ActionRejection::cycle_detected);
    EXPECT_EQ(output[3].outcome, ActionOutcome::no_change);
    EXPECT_EQ(output[4].rejection, ActionRejection::invalid_range);
    EXPECT_EQ(Persistence::serialize(Persistence::capture(world, random)).bytes, before.bytes);
}

TEST(ActionLegacyPersistenceTest, V1AndV2ContinueThroughSameExecutorWithoutInventingSimulationState) {
    for (const auto version : {save_format_version, save_format_version_v2}) {
        WorldState world;
        RandomStreams random{42};
        for (int i = 0; i < 4; ++i) ASSERT_TRUE(world.create_entity().is_valid());
        ASSERT_EQ(world.place_entity(unit, first), ContainmentChange::applied);
        if (version == save_format_version_v2) {
            ASSERT_EQ(world.add_relation({unit, kind, first}), RelationChange::applied);
            ASSERT_EQ(world.declare_population(first, 9), PopulationChange::applied);
        }
        static_cast<void>(random.stream(0).next());
        const auto original = Persistence::capture(world, random);
        ASSERT_EQ(original.format_version, version);
        const auto encoded = Persistence::serialize(original);
        ASSERT_EQ(encoded.status, PersistenceStatus::success);
        WorldState restored;
        RandomStreams restored_random{1};
        ASSERT_EQ(Persistence::load(encoded.bytes, restored, restored_random), PersistenceStatus::success);
        EXPECT_EQ(Persistence::serialize(Persistence::capture(restored, restored_random)).bytes, encoded.bytes);
        EXPECT_TRUE(restored.knowledge_of(actor).empty());
        const std::array actions{
            ActionRequest{actor, PlaceEntityAction{unit, second}},
            ActionRequest{unit, StreamRollAction{0, 0, 100}}};
        std::array<ActionResult, actions.size()> uninterrupted, resumed;
        ASSERT_EQ(Simulation::step(world, random, actions, uninterrupted).processed, actions.size());
        ASSERT_EQ(Simulation::step(restored, restored_random, actions, resumed).processed, actions.size());
        EXPECT_EQ(uninterrupted, resumed);
        EXPECT_EQ(Persistence::capture(world, random), Persistence::capture(restored, restored_random));
    }
}

} // namespace
