#include <corerpg/random.hpp>
#include <corerpg/roll_action.hpp>
#include <corerpg/world_state.hpp>

#include <gtest/gtest.h>

namespace {

TEST(RollActionTest, ExistingEntityProducesValueInsideRange) {
    corerpg::WorldState world;
    const auto entity_id = world.create_entity();
    corerpg::DeterministicRandom random{42};

    const auto result = corerpg::resolve_roll(world, random, {entity_id, 1, 100});

    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(result.status, corerpg::RollStatus::success);
    EXPECT_GE(result.value, 1U);
    EXPECT_LE(result.value, 100U);
}

TEST(RollActionTest, MissingEntityFailsWithoutConsumingRandomness) {
    const corerpg::WorldState world;
    corerpg::DeterministicRandom random{42};
    const auto initial_random_state = random.state();

    const auto result =
        corerpg::resolve_roll(world, random, {corerpg::EntityId{1}, 1, 100});

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.status, corerpg::RollStatus::entity_not_found);
    EXPECT_EQ(random.state(), initial_random_state);
    EXPECT_EQ(world.entity_count(), 0U);
    EXPECT_EQ(world.revision(), 0U);
}

TEST(RollActionTest, InvalidRangeFailsWithoutConsumingRandomness) {
    corerpg::WorldState world;
    const auto entity_id = world.create_entity();
    corerpg::DeterministicRandom random{42};
    const auto initial_random_state = random.state();
    const auto initial_world_revision = world.revision();

    const auto result = corerpg::resolve_roll(world, random, {entity_id, 100, 1});

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.status, corerpg::RollStatus::invalid_range);
    EXPECT_EQ(random.state(), initial_random_state);
    EXPECT_EQ(world.entity_count(), 1U);
    EXPECT_EQ(world.revision(), initial_world_revision);
}

TEST(RollActionTest, FixedRangeSucceedsWithoutConsumingRandomness) {
    corerpg::WorldState world;
    const auto entity_id = world.create_entity();
    corerpg::DeterministicRandom random{42};
    const auto initial_random_state = random.state();

    const auto result = corerpg::resolve_roll(world, random, {entity_id, 17, 17});

    EXPECT_TRUE(result.succeeded());
    EXPECT_EQ(result.value, 17U);
    EXPECT_EQ(random.state(), initial_random_state);
}

TEST(RollActionTest, SameInputsProduceSameResultAndFinalRandomState) {
    corerpg::WorldState world;
    const auto entity_id = world.create_entity();
    corerpg::DeterministicRandom first_random{1234};
    corerpg::DeterministicRandom second_random{1234};
    const corerpg::RollAction action{entity_id, 1, 100};

    const auto first_result = corerpg::resolve_roll(world, first_random, action);
    const auto second_result = corerpg::resolve_roll(world, second_random, action);

    EXPECT_EQ(first_result.status, second_result.status);
    EXPECT_EQ(first_result.value, second_result.value);
    EXPECT_EQ(first_random.state(), second_random.state());
}

TEST(RollActionTest, EquivalentWorldsProduceSameResult) {
    corerpg::WorldState first_world;
    corerpg::WorldState second_world;
    const auto first_entity = first_world.create_entity();
    const auto second_entity = second_world.create_entity();
    corerpg::DeterministicRandom first_random{9876};
    corerpg::DeterministicRandom second_random{9876};

    const auto first_result =
        corerpg::resolve_roll(first_world, first_random, {first_entity, 10, 20});
    const auto second_result =
        corerpg::resolve_roll(second_world, second_random, {second_entity, 10, 20});

    EXPECT_EQ(first_entity, second_entity);
    EXPECT_EQ(first_result.status, second_result.status);
    EXPECT_EQ(first_result.value, second_result.value);
    EXPECT_EQ(first_random.state(), second_random.state());
    EXPECT_EQ(first_world.entity_count(), second_world.entity_count());
    EXPECT_EQ(first_world.revision(), second_world.revision());
}

TEST(RollActionTest, SuccessfulRollDoesNotChangeWorldRevision) {
    corerpg::WorldState world;
    const auto entity_id = world.create_entity();
    corerpg::DeterministicRandom random{42};
    const auto initial_revision = world.revision();

    const auto result = corerpg::resolve_roll(world, random, {entity_id, 1, 100});

    ASSERT_TRUE(result.succeeded());
    EXPECT_EQ(world.revision(), initial_revision);
}

} // namespace
