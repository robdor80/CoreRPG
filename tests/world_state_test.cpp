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
#include <utility>
#include <vector>

namespace {

static_assert(sizeof(corerpg::EntityId::value_type) == 8);
static_assert(std::is_trivially_copyable_v<corerpg::EntityId>);
static_assert(std::totally_ordered<corerpg::EntityId>);
static_assert(!std::is_same_v<corerpg::DefinitionId, corerpg::EntityId>);
static_assert(!std::is_convertible_v<corerpg::DefinitionId, corerpg::EntityId>);
static_assert(!std::is_convertible_v<corerpg::EntityId, corerpg::DefinitionId>);
static_assert([] {
    corerpg::WorldState world;
    return world.entity_count() == 0 && world.revision() == 0 &&
           !world.contains(corerpg::EntityId{1}) &&
           !world.next_entity_id(corerpg::EntityId{}).has_value();
}());
static_assert(noexcept(std::declval<corerpg::WorldState&>().create_entity()));
static_assert(!noexcept(std::declval<corerpg::WorldState&>().place_entity({}, {})));
static_assert(noexcept(std::declval<corerpg::WorldState&>().remove_containment({})));

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

using corerpg::ContainmentChange;
using corerpg::ContainmentStatus;
using corerpg::EntityId;

class WorldStateContainmentTest : public ::testing::Test {
protected:
    corerpg::WorldState world;
    std::array<EntityId, 5> ids{};

    void SetUp() override {
        for (auto& id : ids) {
            id = world.create_entity();
        }
    }

    void expect_link(EntityId entity, EntityId container) const {
        const auto result = world.container_of(entity);
        EXPECT_EQ(result.status, ContainmentStatus::contained);
        EXPECT_EQ(result.container, container);
    }

    void expect_uncontained(EntityId entity) const {
        const auto result = world.container_of(entity);
        EXPECT_EQ(result.status, ContainmentStatus::uncontained);
        EXPECT_FALSE(result.container.is_valid());
    }
};

TEST(WorldStateContainmentQueryTest, EmptyWorldDistinguishesMissingEntities) {
    const corerpg::WorldState world;
    for (const auto entity : {EntityId{}, EntityId{1}}) {
        const auto result = world.container_of(entity);
        EXPECT_EQ(result.status, ContainmentStatus::entity_not_found);
        EXPECT_FALSE(result.container.is_valid());
    }
    EXPECT_EQ(world.revision(), 0U);
}

TEST_F(WorldStateContainmentTest, CreatedEntitiesHaveNoAutomaticLinks) {
    for (const auto entity : ids) {
        expect_uncontained(entity);
    }
    const auto missing = world.container_of(EntityId{99});
    EXPECT_EQ(missing.status, ContainmentStatus::entity_not_found);
    EXPECT_FALSE(missing.container.is_valid());
    EXPECT_EQ(world.entity_count(), ids.size());
    EXPECT_EQ(world.revision(), ids.size());
}

TEST_F(WorldStateContainmentTest, AssociationReassociationAndNoOpPreserveIdentity) {
    const auto initial_revision = world.revision();
    EXPECT_EQ(world.place_entity(ids[0], ids[1]), ContainmentChange::applied);
    expect_link(ids[0], ids[1]);
    EXPECT_EQ(world.revision(), initial_revision + 1U);
    EXPECT_EQ(world.place_entity(ids[0], ids[1]), ContainmentChange::no_change);
    EXPECT_EQ(world.revision(), initial_revision + 1U);
    EXPECT_EQ(world.place_entity(ids[0], ids[2]), ContainmentChange::applied);
    expect_link(ids[0], ids[2]);
    expect_uncontained(ids[1]);
    EXPECT_EQ(world.revision(), initial_revision + 2U);
    EXPECT_EQ(world.entity_count(), ids.size());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        EXPECT_TRUE(world.contains(ids[i]));
        EXPECT_EQ(world.next_entity_id(EntityId{i}), ids[i]);
    }
    const auto created = world.create_entity();
    EXPECT_EQ(created, EntityId{6});
    expect_uncontained(created);
    EXPECT_EQ(world.revision(), initial_revision + 3U);
}

TEST_F(WorldStateContainmentTest, WithdrawalRemovesOnlyTheDirectLink) {
    ASSERT_EQ(world.place_entity(ids[0], ids[1]), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(ids[1], ids[2]), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(ids[3], ids[2]), ContainmentChange::applied);
    const auto revision = world.revision();
    EXPECT_EQ(world.remove_containment(ids[1]), ContainmentChange::applied);
    expect_uncontained(ids[1]);
    expect_link(ids[0], ids[1]);
    expect_link(ids[3], ids[2]);
    EXPECT_EQ(world.revision(), revision + 1U);
    EXPECT_EQ(world.remove_containment(ids[1]), ContainmentChange::no_change);
    EXPECT_EQ(world.remove_containment(ids[4]), ContainmentChange::no_change);
    EXPECT_EQ(world.remove_containment(EntityId{}), ContainmentChange::entity_not_found);
    EXPECT_EQ(world.remove_containment(EntityId{99}), ContainmentChange::entity_not_found);
    EXPECT_EQ(world.revision(), revision + 1U);
    EXPECT_EQ(world.entity_count(), ids.size());
    for (const auto entity : ids) {
        EXPECT_TRUE(world.contains(entity));
    }
}

TEST_F(WorldStateContainmentTest, InvalidPlacementHasStablePrecedenceAndPreservesLinks) {
    ASSERT_EQ(world.place_entity(ids[0], ids[1]), ContainmentChange::applied);
    const auto revision = world.revision();
    struct InvalidPlacement {
        EntityId entity;
        EntityId container;
        ContainmentChange expected;
    };
    const std::array cases{
        InvalidPlacement{{}, {}, ContainmentChange::entity_not_found},
        InvalidPlacement{EntityId{99}, EntityId{99}, ContainmentChange::entity_not_found},
        InvalidPlacement{{}, ids[0], ContainmentChange::entity_not_found},
        InvalidPlacement{ids[0], {}, ContainmentChange::container_not_found},
        InvalidPlacement{ids[0], EntityId{99}, ContainmentChange::container_not_found},
        InvalidPlacement{ids[0], ids[0], ContainmentChange::self_containment},
    };
    for (const auto& test : cases) {
        EXPECT_EQ(world.place_entity(test.entity, test.container), test.expected);
        expect_link(ids[0], ids[1]);
        for (std::size_t i = 1; i < ids.size(); ++i) {
            expect_uncontained(ids[i]);
        }
        EXPECT_EQ(world.revision(), revision);
        EXPECT_EQ(world.entity_count(), ids.size());
    }
}

TEST_F(WorldStateContainmentTest, DirectAndIndirectCyclesAreRejectedAtomically) {
    for (std::size_t i = 0; i < 3; ++i) {
        ASSERT_EQ(world.place_entity(ids[i], ids[i + 1]), ContainmentChange::applied);
    }
    ASSERT_EQ(world.place_entity(ids[3], ids[4]), ContainmentChange::applied);
    const auto revision = world.revision();
    EXPECT_EQ(world.place_entity(ids[1], ids[0]), ContainmentChange::cycle_detected);
    EXPECT_EQ(world.place_entity(ids[3], ids[0]), ContainmentChange::cycle_detected);
    EXPECT_EQ(world.place_entity(ids[4], ids[0]), ContainmentChange::cycle_detected);
    for (std::size_t i = 0; i < 4; ++i) {
        expect_link(ids[i], ids[i + 1]);
    }
    expect_uncontained(ids[4]);
    EXPECT_EQ(world.revision(), revision);
    EXPECT_EQ(world.entity_count(), ids.size());
}

TEST_F(WorldStateContainmentTest, MovingAContainerPreservesAllItsDirectContents) {
    ASSERT_EQ(world.place_entity(ids[0], ids[2]), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(ids[1], ids[2]), ContainmentChange::applied);
    ASSERT_EQ(world.place_entity(ids[2], ids[3]), ContainmentChange::applied);
    const auto revision = world.revision();
    EXPECT_EQ(world.place_entity(ids[2], ids[4]), ContainmentChange::applied);
    expect_link(ids[0], ids[2]);
    expect_link(ids[1], ids[2]);
    expect_link(ids[2], ids[4]);
    expect_uncontained(ids[3]);
    EXPECT_EQ(world.revision(), revision + 1U);
}

TEST_F(WorldStateContainmentTest, QueriesAndMutationsRemainIndependentOfRandomAndContent) {
    corerpg::DeterministicRandom random{42};
    const auto random_state = random.state();
    ASSERT_EQ(world.place_entity(ids[0], ids[1]), ContainmentChange::applied);
    const auto revision = world.revision();
    for (const auto entity : ids) {
        static_cast<void>(world.container_of(entity));
        static_cast<void>(world.contains(entity));
        static_cast<void>(world.next_entity_id(entity));
    }
    EXPECT_EQ(world.revision(), revision);
    EXPECT_EQ(world.place_entity(ids[0], ids[2]), ContainmentChange::applied);
    EXPECT_EQ(world.remove_containment(ids[0]), ContainmentChange::applied);
    EXPECT_EQ(random.state(), random_state);
    // All operations work in a core-only world without any ContentCatalog.
}

TEST_F(WorldStateContainmentTest, CopyConstructionAndAssignmentHaveIndependentValueSemantics) {
    ASSERT_EQ(world.place_entity(ids[0], ids[1]), ContainmentChange::applied);
    const auto initial_revision = world.revision();
    auto copy = world;
    corerpg::WorldState assigned;
    static_cast<void>(assigned.create_entity());
    assigned = world;
    EXPECT_EQ(copy.entity_count(), world.entity_count());
    EXPECT_EQ(assigned.revision(), initial_revision);
    EXPECT_EQ(copy.container_of(ids[0]).container, ids[1]);
    EXPECT_EQ(assigned.container_of(ids[0]).container, ids[1]);
    EXPECT_EQ(copy.place_entity(ids[0], ids[2]), ContainmentChange::applied);
    EXPECT_EQ(assigned.remove_containment(ids[0]), ContainmentChange::applied);
    expect_link(ids[0], ids[1]);
    EXPECT_EQ(copy.container_of(ids[0]).container, ids[2]);
    EXPECT_EQ(assigned.container_of(ids[0]).status, ContainmentStatus::uncontained);
    EXPECT_EQ(world.revision(), initial_revision);
    EXPECT_EQ(copy.revision(), initial_revision + 1U);
    EXPECT_EQ(assigned.revision(), initial_revision + 1U);
    auto& self = world;
    world = self;
    expect_link(ids[0], ids[1]);
    EXPECT_EQ(world.revision(), initial_revision);
    EXPECT_EQ(copy.create_entity(), EntityId{6});
    EXPECT_EQ(world.entity_count(), 5U);
    EXPECT_EQ(assigned.entity_count(), 5U);
}

TEST(WorldStateContainmentQueryTest, IndependentWorldsProduceCanonicalLinksForTheSameOperations) {
    corerpg::WorldState first;
    corerpg::WorldState second;
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(first.create_entity(), second.create_entity());
    }
    const std::array operations{
        std::pair{EntityId{4}, EntityId{5}},
        std::pair{EntityId{1}, EntityId{3}},
        std::pair{EntityId{2}, EntityId{3}},
        std::pair{EntityId{3}, EntityId{4}},
        std::pair{EntityId{1}, EntityId{4}},
        std::pair{EntityId{5}, EntityId{1}}, // Rejected cycle.
        std::pair{EntityId{2}, EntityId{3}}, // No-op.
    };
    for (const auto& [entity, container] : operations) {
        EXPECT_EQ(first.place_entity(entity, container), second.place_entity(entity, container));
    }
    const auto enumerate = [](const corerpg::WorldState& world) {
        std::vector<std::pair<EntityId, EntityId>> links;
        EntityId cursor;
        while (const auto next = world.next_entity_id(cursor)) {
            cursor = *next;
            const auto result = world.container_of(cursor);
            if (result.status == ContainmentStatus::contained) {
                links.emplace_back(cursor, result.container);
            }
        }
        return links;
    };
    const std::vector expected{
        std::pair{EntityId{1}, EntityId{4}}, std::pair{EntityId{2}, EntityId{3}},
        std::pair{EntityId{3}, EntityId{4}}, std::pair{EntityId{4}, EntityId{5}},
    };
    EXPECT_EQ(enumerate(first), expected);
    EXPECT_EQ(enumerate(second), expected);
    EXPECT_EQ(first.revision(), second.revision());
    const auto second_revision = second.revision();
    EXPECT_EQ(first.remove_containment(EntityId{4}), ContainmentChange::applied);
    EXPECT_EQ(second.revision(), second_revision);
    EXPECT_EQ(enumerate(second), expected);
}

TEST(WorldStateContainmentTransitionTest, ExhaustedRevisionPreservesLinksAndErrorPrecedence) {
    corerpg::detail::ContainmentLinks links{
        {EntityId{1}, EntityId{2}}, {EntityId{2}, EntityId{3}},
    };
    const auto original = links;
    auto revision = std::numeric_limits<std::uint64_t>::max();
    const auto place = [&](EntityId entity, EntityId container) {
        return corerpg::detail::place_entity(links, 4U, revision, entity, container);
    };
    EXPECT_EQ(place({}, {}), ContainmentChange::entity_not_found);
    EXPECT_EQ(place(EntityId{1}, {}), ContainmentChange::container_not_found);
    EXPECT_EQ(place(EntityId{1}, EntityId{1}), ContainmentChange::self_containment);
    EXPECT_EQ(place(EntityId{3}, EntityId{1}), ContainmentChange::cycle_detected);
    EXPECT_EQ(place(EntityId{1}, EntityId{2}), ContainmentChange::no_change);
    EXPECT_EQ(place(EntityId{1}, EntityId{4}), ContainmentChange::revision_exhausted);
    EXPECT_EQ(place(EntityId{4}, EntityId{3}), ContainmentChange::revision_exhausted);
    EXPECT_EQ(corerpg::detail::remove_containment(links, 4U, revision, {}),
              ContainmentChange::entity_not_found);
    EXPECT_EQ(corerpg::detail::remove_containment(links, 4U, revision, EntityId{4}),
              ContainmentChange::no_change);
    EXPECT_EQ(corerpg::detail::remove_containment(links, 4U, revision, EntityId{1}),
              ContainmentChange::revision_exhausted);
    EXPECT_EQ(links, original);
    EXPECT_EQ(revision, std::numeric_limits<std::uint64_t>::max());
    EntityId::value_type last_entity_id = 4U;
    EXPECT_FALSE(corerpg::detail::advance_entity_creation(last_entity_id, revision).is_valid());
    EXPECT_EQ(last_entity_id, 4U);
    EXPECT_EQ(revision, std::numeric_limits<std::uint64_t>::max());
}

TEST(WorldStateContainmentTransitionTest, EachMutationCanReachMaximumRevisionThenRejectsChanges) {
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (int operation = 0; operation < 3; ++operation) {
        SCOPED_TRACE(operation);
        corerpg::detail::ContainmentLinks links;
        if (operation != 0) {
            links.emplace_back(EntityId{1}, EntityId{2});
        }
        auto revision = maximum - 1U;
        const auto result = operation == 2
            ? corerpg::detail::remove_containment(links, 3U, revision, EntityId{1})
            : corerpg::detail::place_entity(links, 3U, revision, EntityId{1}, EntityId{3});
        EXPECT_EQ(result, ContainmentChange::applied);
        EXPECT_EQ(revision, maximum);
        const auto saved = links;
        EXPECT_EQ(corerpg::detail::place_entity(links, 3U, revision, EntityId{1}, EntityId{2}),
                  ContainmentChange::revision_exhausted);
        EXPECT_EQ(links, saved);
        EXPECT_EQ(revision, maximum);
    }
}

} // namespace
