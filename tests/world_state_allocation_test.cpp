#include <corerpg/world_state.hpp>

#include "../src/world_state_internal.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <new>

namespace {

// This executable alone replaces ordinary allocation. Failure is scoped, one-shot,
// and armed only around the production call, never around GoogleTest assertions.
thread_local bool fail_next_allocation = false;
thread_local bool allocation_failed = false;

class AllocationFailure final {
public:
    AllocationFailure() noexcept {
        allocation_failed = false;
        fail_next_allocation = true;
    }
    ~AllocationFailure() { fail_next_allocation = false; }
};

} // namespace

void* operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        allocation_failed = true;
        throw std::bad_alloc{};
    }
    if (auto* allocation = std::malloc(size == 0 ? 1 : size)) {
        return allocation;
    }
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* allocation) noexcept { std::free(allocation); }
void operator delete[](void* allocation) noexcept { std::free(allocation); }
void operator delete(void* allocation, std::size_t) noexcept { std::free(allocation); }
void operator delete[](void* allocation, std::size_t) noexcept { std::free(allocation); }

namespace {

using corerpg::ContainmentChange;
using corerpg::ContainmentStatus;
using corerpg::EntityId;

TEST(WorldStateAllocationTest, FailedFirstAssociationPreservesTheEntireWorld) {
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    const auto container = world.create_entity();
    const auto revision = world.revision();
    bool threw = false;
    {
        AllocationFailure failure;
        try {
            static_cast<void>(world.place_entity(entity, container));
        } catch (const std::bad_alloc&) {
            threw = true;
        }
    }
    EXPECT_TRUE(threw);
    EXPECT_TRUE(allocation_failed);
    EXPECT_EQ(world.revision(), revision);
    EXPECT_EQ(world.entity_count(), 2U);
    EXPECT_EQ(world.container_of(entity).status, ContainmentStatus::uncontained);
    EXPECT_EQ(world.container_of(container).status, ContainmentStatus::uncontained);
    EXPECT_EQ(world.next_entity_id({}), entity);
    EXPECT_EQ(world.next_entity_id(entity), container);
    EXPECT_EQ(world.create_entity(), EntityId{3});
    EXPECT_EQ(world.place_entity(entity, container), ContainmentChange::applied);
}

TEST(WorldStateAllocationTest, FailedInsertionPreservesAllExistingLinksAndRevision) {
    for (const bool insert_before : {false, true}) {
        SCOPED_TRACE(insert_before);
        corerpg::detail::ContainmentLinks links;
        links.reserve(3);
        const auto capacity = links.capacity();
        const EntityId container{capacity + 2U};
        for (std::size_t i = 0; i < capacity; ++i) {
            links.emplace_back(EntityId{i + (insert_before ? 2U : 1U)}, container);
        }
        const EntityId entity{insert_before ? 1U : capacity + 1U};
        const auto original = links;
        std::uint64_t revision = 17;
        bool threw = false;
        {
            AllocationFailure failure;
            try {
                static_cast<void>(corerpg::detail::place_entity(
                    links, container.value(), revision, entity, container));
            } catch (const std::bad_alloc&) {
                threw = true;
            }
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(allocation_failed);
        EXPECT_EQ(links, original);
        EXPECT_EQ(revision, 17U);
    }
}

TEST(WorldStateAllocationTest, ReassociationAndWithdrawalDoNotAllocate) {
    corerpg::WorldState world;
    const auto entity = world.create_entity();
    const auto first = world.create_entity();
    const auto second = world.create_entity();
    ASSERT_EQ(world.place_entity(entity, first), ContainmentChange::applied);
    ContainmentChange reassociation{};
    ContainmentChange withdrawal{};
    {
        AllocationFailure failure;
        reassociation = world.place_entity(entity, second);
        withdrawal = world.remove_containment(entity);
    }
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(reassociation, ContainmentChange::applied);
    EXPECT_EQ(withdrawal, ContainmentChange::applied);
    EXPECT_EQ(world.revision(), 6U);
    EXPECT_EQ(world.container_of(entity).status, ContainmentStatus::uncontained);
}

TEST(WorldStateAllocationTest, FailedCopyAssignmentPreservesBothCompleteWorlds) {
    corerpg::WorldState source;
    const auto first = source.create_entity();
    const auto second = source.create_entity();
    const auto third = source.create_entity();
    ASSERT_EQ(source.place_entity(first, second), ContainmentChange::applied);
    ASSERT_EQ(source.place_entity(second, third), ContainmentChange::applied);
    corerpg::WorldState target;
    const auto target_first = target.create_entity();
    const auto target_second = target.create_entity();
    ASSERT_EQ(target.place_entity(target_second, target_first), ContainmentChange::applied);
    bool threw = false;
    {
        AllocationFailure failure;
        try {
            target = source;
        } catch (const std::bad_alloc&) {
            threw = true;
        }
    }
    EXPECT_TRUE(threw);
    EXPECT_TRUE(allocation_failed);
    EXPECT_EQ(target.entity_count(), 2U);
    EXPECT_EQ(target.revision(), 3U);
    EXPECT_EQ(target.container_of(target_first).status, ContainmentStatus::uncontained);
    EXPECT_EQ(target.container_of(target_second).container, target_first);
    EXPECT_EQ(source.entity_count(), 3U);
    EXPECT_EQ(source.revision(), 5U);
    EXPECT_EQ(source.container_of(first).container, second);
    EXPECT_EQ(source.container_of(second).container, third);
    EXPECT_EQ(target.create_entity(), EntityId{3});
}

TEST(WorldStateAllocationTest, FailedCopyConstructionPreservesTheSource) {
    corerpg::WorldState source;
    const auto entity = source.create_entity();
    const auto container = source.create_entity();
    ASSERT_EQ(source.place_entity(entity, container), ContainmentChange::applied);
    bool threw = false;
    {
        AllocationFailure failure;
        try {
            const auto copy = source;
            static_cast<void>(copy);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
    }
    EXPECT_TRUE(threw);
    EXPECT_TRUE(allocation_failed);
    EXPECT_EQ(source.entity_count(), 2U);
    EXPECT_EQ(source.revision(), 3U);
    EXPECT_EQ(source.container_of(entity).container, container);
}

} // namespace
