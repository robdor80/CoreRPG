#include <corerpg/definition_id.hpp>
#include <corerpg/entity_id.hpp>
#include <corerpg/world_state.hpp>

#include <gtest/gtest.h>

#include <array>
#include <concepts>
#include <cstddef>
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

} // namespace
