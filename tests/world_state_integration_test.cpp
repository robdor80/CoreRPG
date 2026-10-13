#include <corerpg/random.hpp>
#include <corerpg/roll_action.hpp>
#include <corerpg/world_state.hpp>

#include <gtest/gtest.h>

#include <array>
#include <tuple>
#include <vector>

namespace {

using corerpg::ContainmentChange;
using corerpg::ContainmentStatus;
using corerpg::EntityId;
using corerpg::WorldState;

// Observe all logical state through the public API, without a serialization format.
auto observe_world(const WorldState& world) {
    std::vector<std::tuple<EntityId, ContainmentStatus, EntityId>> entities;
    EntityId cursor;
    while (const auto next = world.next_entity_id(cursor)) {
        EXPECT_GT(*next, cursor);
        EXPECT_TRUE(world.contains(*next));
        cursor = *next;
        const auto result = world.container_of(cursor);
        EXPECT_NE(result.status, ContainmentStatus::entity_not_found);
        entities.emplace_back(cursor, result.status, result.container);
    }
    return std::tuple{world.entity_count(), world.revision(), entities};
}

TEST(WorldStateIntegrationTest, NestedContainmentReassociationCyclesAndWithdrawalPreserveState) {
    WorldState world;
    const std::array ids{
        world.create_entity(), world.create_entity(), world.create_entity(), world.create_entity(),
    };
    const std::array expected_ids{EntityId{1}, EntityId{2}, EntityId{3}, EntityId{4}};
    ASSERT_EQ(ids, expected_ids);
    const auto [a, b, c, d] = ids;

    ASSERT_EQ(world.place_entity(a, b), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(b, c), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(c, d), ContainmentChange::applied);
    std::vector<std::tuple<EntityId, ContainmentStatus, EntityId>> expected_entities{
        {a, ContainmentStatus::contained, b}, {b, ContainmentStatus::contained, c},
        {c, ContainmentStatus::contained, d}, {d, ContainmentStatus::uncontained, EntityId{}},
    };
    EXPECT_EQ(std::get<2>(observe_world(world)), expected_entities);
    EXPECT_EQ(world.entity_count(), 4U);
    EXPECT_EQ(world.revision(), 7U);

    ASSERT_EQ(world.place_entity(a, c), ContainmentChange::applied);
    expected_entities[0] = {a, ContainmentStatus::contained, c};
    EXPECT_EQ(std::get<2>(observe_world(world)), expected_entities);
    EXPECT_EQ(world.entity_count(), 4U);
    EXPECT_EQ(world.revision(), 8U);

    const auto before_rejections = observe_world(world);
    // Current topology: A -> C -> D and B -> C -> D.
    // D -> A closes A -> C -> D -> A; C -> A closes A -> C -> A.
    EXPECT_EQ(world.place_entity(d, a), ContainmentChange::cycle_detected);
    EXPECT_EQ(observe_world(world), before_rejections);
    EXPECT_EQ(world.place_entity(c, a), ContainmentChange::cycle_detected);
    EXPECT_EQ(observe_world(world), before_rejections);
    EXPECT_EQ(world.place_entity(a, c), ContainmentChange::no_change);
    EXPECT_EQ(observe_world(world), before_rejections);
    EXPECT_EQ(world.place_entity(EntityId{99}, d), ContainmentChange::entity_not_found);
    EXPECT_EQ(observe_world(world), before_rejections);

    // Restore a descendant of B so withdrawal preservation is actually exercised.
    ASSERT_EQ(world.place_entity(a, b), ContainmentChange::applied);
    EXPECT_EQ(world.revision(), 9U);
    ASSERT_EQ(world.remove_containment(b), ContainmentChange::applied);
    expected_entities[0] = {a, ContainmentStatus::contained, b};
    expected_entities[1] = {b, ContainmentStatus::uncontained, EntityId{}};
    EXPECT_EQ(std::get<2>(observe_world(world)), expected_entities);
    EXPECT_EQ(world.entity_count(), 4U);
    EXPECT_EQ(world.revision(), 10U);
    const auto after_withdrawal = observe_world(world);
    EXPECT_EQ(world.remove_containment(b), ContainmentChange::no_change);
    EXPECT_EQ(observe_world(world), after_withdrawal);

    const auto e = world.create_entity();
    EXPECT_EQ(e, EntityId{5});
    expected_entities.emplace_back(e, ContainmentStatus::uncontained, EntityId{});
    EXPECT_EQ(std::get<2>(observe_world(world)), expected_entities);
    EXPECT_EQ(world.entity_count(), 5U);
    EXPECT_EQ(world.revision(), 11U);
}

TEST(WorldStateIntegrationTest, MultilevelCopiesKeepLinksRevisionsAndFutureIdentitiesIndependent) {
    WorldState original;
    const auto a = original.create_entity();
    const auto b = original.create_entity();
    const auto c = original.create_entity();
    const auto d = original.create_entity();
    ASSERT_EQ(original.place_entity(a, b), ContainmentChange::applied);
    ASSERT_EQ(original.place_entity(b, c), ContainmentChange::applied);
    ASSERT_EQ(original.place_entity(c, d), ContainmentChange::applied);
    const auto original_state = observe_world(original);
    auto copy = original;
    WorldState assigned;
    assigned = original;
    EXPECT_EQ(observe_world(copy), original_state);
    EXPECT_EQ(observe_world(assigned), original_state);

    ASSERT_EQ(copy.place_entity(b, d), ContainmentChange::applied);
    const auto copy_e = copy.create_entity();
    ASSERT_EQ(copy.place_entity(copy_e, a), ContainmentChange::applied);
    EXPECT_EQ(copy_e, EntityId{5});
    EXPECT_EQ(copy.entity_count(), 5U);
    EXPECT_EQ(copy.revision(), 10U);
    EXPECT_EQ(copy.container_of(a).container, b);
    EXPECT_EQ(copy.container_of(b).container, d);
    EXPECT_EQ(copy.container_of(c).container, d);
    EXPECT_EQ(observe_world(original), original_state);
    EXPECT_EQ(observe_world(assigned), original_state);

    ASSERT_EQ(assigned.remove_containment(c), ContainmentChange::applied);
    EXPECT_EQ(assigned.container_of(a).container, b);
    EXPECT_EQ(assigned.container_of(b).container, c);
    EXPECT_EQ(assigned.container_of(c).status, ContainmentStatus::uncontained);
    EXPECT_EQ(assigned.revision(), 8U);
    EXPECT_FALSE(assigned.contains(copy_e));
    EXPECT_EQ(observe_world(original), original_state);
    const auto copy_state = observe_world(copy);
    const auto assigned_state = observe_world(assigned);

    const auto original_e = original.create_entity();
    EXPECT_EQ(original_e, copy_e); // Equal numeric IDs belong to independent worlds.
    EXPECT_EQ(original.create_entity(), EntityId{6});
    EXPECT_EQ(original.revision(), 9U);
    EXPECT_EQ(original.container_of(original_e).status, ContainmentStatus::uncontained);
    EXPECT_EQ(observe_world(copy), copy_state);
    EXPECT_EQ(observe_world(assigned), assigned_state);
    EXPECT_EQ(copy.create_entity(), EntityId{6});
    EXPECT_EQ(assigned.create_entity(), EntityId{5});
}

TEST(WorldStateIntegrationTest, RollActionIgnoresContainmentAndPreservesPersistentFacts) {
    WorldState contained;
    WorldState uncontained;
    std::array<EntityId, 3> ids;
    for (auto& id : ids) {
        id = contained.create_entity();
        ASSERT_EQ(uncontained.create_entity(), id);
    }
    ASSERT_EQ(contained.place_entity(ids[0], ids[1]), ContainmentChange::applied);
    ASSERT_EQ(contained.place_entity(ids[1], ids[2]), ContainmentChange::applied);
    corerpg::DeterministicRandom first_random{9876};
    corerpg::DeterministicRandom second_random{9876};

    for (int step = 0; step < 2; ++step) {
        SCOPED_TRACE(step);
        const auto before = observe_world(contained);
        const auto other_before = observe_world(uncontained);
        const auto first = corerpg::resolve_roll(contained, first_random, {ids[0], 1, 100});
        const auto second = corerpg::resolve_roll(uncontained, second_random, {ids[0], 1, 100});
        EXPECT_EQ(first.status, corerpg::RollStatus::success);
        EXPECT_EQ(first.status, second.status);
        EXPECT_EQ(first.value, second.value);
        EXPECT_EQ(first_random.state(), second_random.state());
        EXPECT_EQ(observe_world(contained), before);
        EXPECT_EQ(observe_world(uncontained), other_before);
        if (step == 0) {
            ASSERT_EQ(contained.place_entity(ids[0], ids[2]), ContainmentChange::applied);
        }
    }

    const auto before = observe_world(contained);
    const auto random_state = first_random.state();
    EXPECT_EQ(corerpg::resolve_roll(contained, first_random, {EntityId{99}, 1, 100}).status,
              corerpg::RollStatus::entity_not_found);
    EXPECT_EQ(corerpg::resolve_roll(contained, first_random, {ids[0], 100, 1}).status,
              corerpg::RollStatus::invalid_range);
    EXPECT_EQ(first_random.state(), random_state);
    EXPECT_EQ(observe_world(contained), before);
}

} // namespace
