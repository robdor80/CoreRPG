#include <corerpg/definition_id.hpp>
#include <corerpg/entity_id.hpp>
#include <corerpg/random.hpp>
#include <corerpg/world_state.hpp>

#include "../src/world_state_internal.hpp"

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace {

static_assert(sizeof(corerpg::EntityId::value_type) == 8);
static_assert(std::is_trivially_copyable_v<corerpg::EntityId>);
static_assert(std::totally_ordered<corerpg::EntityId>);
static_assert(!std::is_same_v<corerpg::DefinitionId, corerpg::EntityId>);
static_assert(!std::is_convertible_v<corerpg::DefinitionId, corerpg::EntityId>);
static_assert(!std::is_convertible_v<corerpg::EntityId, corerpg::DefinitionId>);

TEST(WorldStateTest, NewWorldIsEmpty) {
    const corerpg::WorldState world;
    const corerpg::EntityId syntactically_valid_id{1};

    EXPECT_EQ(world.entity_count(), 0U);
    EXPECT_EQ(world.revision(), 0U);
    EXPECT_FALSE(world.contains(corerpg::EntityId{}));
    EXPECT_TRUE(syntactically_valid_id.is_valid());
    EXPECT_FALSE(world.contains(syntactically_valid_id));
}

TEST(WorldStateTest, FirstEntityReceivesFirstValidId) {
    corerpg::WorldState world;
    const auto initial_count = world.entity_count();
    const auto initial_revision = world.revision();

    const auto entity_id = world.create_entity();

    EXPECT_EQ(entity_id, corerpg::EntityId{1});
    EXPECT_TRUE(entity_id.is_valid());
    EXPECT_EQ(world.entity_count(), initial_count + 1U);
    EXPECT_EQ(world.revision(), initial_revision + 1U);
}

TEST(WorldStateTest, CreatedIdsAreDistinctAndMonotonic) {
    corerpg::WorldState world;

    const auto first = world.create_entity();
    const auto second = world.create_entity();
    const auto third = world.create_entity();

    EXPECT_NE(first, second);
    EXPECT_NE(second, third);
    EXPECT_LT(first, second);
    EXPECT_LT(second, third);
}

TEST(WorldStateTest, ContainsRecognizesOnlyCreatedEntities) {
    corerpg::WorldState world;
    const auto entity_id = world.create_entity();

    EXPECT_TRUE(world.contains(entity_id));
    EXPECT_FALSE(world.contains(corerpg::EntityId{}));
    EXPECT_FALSE(world.contains(corerpg::EntityId{2}));
}

TEST(WorldStateTest, EntityCountTracksCreations) {
    corerpg::WorldState world;

    static_cast<void>(world.create_entity());
    static_cast<void>(world.create_entity());
    static_cast<void>(world.create_entity());

    EXPECT_EQ(world.entity_count(), 3U);
}

TEST(WorldStateTest, EntityIdsAreScopedToWorldAndIndependentWorldsMayReuseValues) {
    corerpg::WorldState first_world;
    corerpg::WorldState second_world;
    std::array<corerpg::EntityId, 3> first_sequence;
    std::array<corerpg::EntityId, 3> second_sequence;

    for (std::size_t index = 0; index < first_sequence.size(); ++index) {
        first_sequence[index] = first_world.create_entity();
        second_sequence[index] = second_world.create_entity();
    }

    EXPECT_EQ(first_sequence, second_sequence);
    EXPECT_EQ(first_sequence.front(), corerpg::EntityId{1});
}

TEST(WorldStateTest, RevisionChangesOnlyAfterMutations) {
    corerpg::WorldState world;

    EXPECT_EQ(world.revision(), 0U);

    static_cast<void>(world.contains(corerpg::EntityId{1}));
    static_cast<void>(world.entity_count());
    EXPECT_EQ(world.revision(), 0U);

    static_cast<void>(world.create_entity());
    EXPECT_EQ(world.revision(), 1U);

    static_cast<void>(world.create_entity());
    EXPECT_EQ(world.revision(), 2U);
}

TEST(WorldStateCreationTest, EachCounterCanReachItsMaximumWithoutWrapping) {
    constexpr auto maximum = std::numeric_limits<corerpg::EntityId::value_type>::max();
    const std::array initial_states{
        std::array<corerpg::EntityId::value_type, 2>{maximum - 1U, 7U},
        std::array<corerpg::EntityId::value_type, 2>{7U, maximum - 1U},
        std::array<corerpg::EntityId::value_type, 2>{maximum - 1U, maximum - 1U},
    };

    for (const auto& initial : initial_states) {
        SCOPED_TRACE(::testing::Message() << "last ID=" << initial[0]
                                        << ", revision=" << initial[1]);
        auto last_entity_id = initial[0];
        auto revision = initial[1];
        const auto result = corerpg::detail::advance_entity_creation(last_entity_id, revision);

        EXPECT_TRUE(result.is_valid());
        EXPECT_EQ(result.value(), initial[0] + 1U);
        EXPECT_EQ(last_entity_id, initial[0] + 1U);
        EXPECT_EQ(revision, initial[1] + 1U);

        EXPECT_FALSE(corerpg::detail::advance_entity_creation(last_entity_id, revision).is_valid());
        EXPECT_EQ(last_entity_id, initial[0] + 1U);
        EXPECT_EQ(revision, initial[1] + 1U);
    }
}

TEST(WorldStateCreationTest, ExhaustedIdOrRevisionRejectsWithoutChangingEitherCounter) {
    constexpr auto maximum = std::numeric_limits<corerpg::EntityId::value_type>::max();
    const std::array initial_states{
        std::array<corerpg::EntityId::value_type, 2>{maximum, 0U},
        std::array<corerpg::EntityId::value_type, 2>{0U, maximum},
        std::array<corerpg::EntityId::value_type, 2>{maximum, maximum},
    };

    for (const auto& initial : initial_states) {
        SCOPED_TRACE(::testing::Message() << "last ID=" << initial[0]
                                        << ", revision=" << initial[1]);
        auto last_entity_id = initial[0];
        auto revision = initial[1];
        for (int attempt = 0; attempt < 2; ++attempt) {
            const auto result = corerpg::detail::advance_entity_creation(last_entity_id, revision);
            EXPECT_EQ(result, corerpg::EntityId{});
            EXPECT_EQ(last_entity_id, initial[0]);
            EXPECT_EQ(revision, initial[1]);
        }
    }
}

TEST(WorldStateTest, EmptyWorldHasNoNextEntityForAnyCursor) {
    const corerpg::WorldState world;
    const std::array cursors{
        corerpg::EntityId{}, corerpg::EntityId{1},
        corerpg::EntityId{std::numeric_limits<corerpg::EntityId::value_type>::max()},
    };
    for (const auto cursor : cursors) {
        EXPECT_FALSE(world.next_entity_id(cursor).has_value());
    }
    EXPECT_EQ(world.entity_count(), 0U);
    EXPECT_EQ(world.revision(), 0U);
}

TEST(WorldStateTest, EnumerationReturnsCreatedEntitiesInStrictAscendingOrder) {
    corerpg::WorldState world;
    std::array<corerpg::EntityId, 4> created;
    for (auto& entity : created) {
        entity = world.create_entity();
    }
    const auto initial_revision = world.revision();
    corerpg::DeterministicRandom random{42};
    const auto initial_random_state = random.state();
    corerpg::EntityId cursor;

    for (const auto expected : created) {
        const auto next = world.next_entity_id(cursor);
        ASSERT_TRUE(next.has_value());
        EXPECT_EQ(*next, expected);
        EXPECT_GT(*next, cursor);
        EXPECT_TRUE(world.contains(*next));
        cursor = *next;
    }
    EXPECT_FALSE(world.next_entity_id(cursor).has_value());
    EXPECT_EQ(world.next_entity_id(created[1]), created[2]);
    EXPECT_FALSE(world.next_entity_id(corerpg::EntityId{99}).has_value());
    EXPECT_FALSE(world.next_entity_id(
        corerpg::EntityId{std::numeric_limits<corerpg::EntityId::value_type>::max()}).has_value());
    EXPECT_EQ(world.entity_count(), created.size());
    EXPECT_EQ(world.revision(), initial_revision);
    EXPECT_EQ(random.state(), initial_random_state);
}

TEST(WorldStateTest, EquivalentWorldsEnumerateEquallyAndQueriesObserveLaterCreations) {
    corerpg::WorldState first;
    corerpg::WorldState second;
    const auto first_id = first.create_entity();
    EXPECT_EQ(second.create_entity(), first_id);
    EXPECT_EQ(first.next_entity_id(corerpg::EntityId{}), first_id);
    EXPECT_EQ(first.next_entity_id(corerpg::EntityId{}), second.next_entity_id(corerpg::EntityId{}));
    EXPECT_FALSE(first.next_entity_id(first_id).has_value());

    const auto later_id = first.create_entity();
    EXPECT_EQ(second.create_entity(), later_id);
    EXPECT_EQ(first.next_entity_id(first_id), later_id);

    corerpg::EntityId cursor;
    for (const auto expected : {first_id, later_id}) {
        const auto next = first.next_entity_id(cursor);
        ASSERT_TRUE(next.has_value());
        EXPECT_EQ(next, second.next_entity_id(cursor));
        EXPECT_EQ(*next, expected);
        cursor = *next;
    }
    EXPECT_FALSE(first.next_entity_id(cursor).has_value());
    EXPECT_FALSE(second.next_entity_id(cursor).has_value());
    EXPECT_EQ(first.revision(), 2U);
    EXPECT_EQ(second.revision(), 2U);
}

} // namespace
