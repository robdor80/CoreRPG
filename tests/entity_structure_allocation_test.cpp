#include <corerpg/persistence.hpp>

#include "entity_structure_helpers.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <new>

namespace {

thread_local std::size_t allocations_before_failure = std::numeric_limits<std::size_t>::max();
thread_local bool allocation_failed = false;

class AllocationFailure final {
  public:
    explicit AllocationFailure(std::size_t index) noexcept {
        allocation_failed = false;
        allocations_before_failure = index;
    }
    ~AllocationFailure() {
        allocations_before_failure = std::numeric_limits<std::size_t>::max();
    }
};

} // namespace

// Isolated executable, ordinary allocation only. Never armed during assertions.
void* operator new(std::size_t size) {
    if (allocations_before_failure != std::numeric_limits<std::size_t>::max()) {
        if (allocations_before_failure == 0) {
            allocations_before_failure = std::numeric_limits<std::size_t>::max();
            allocation_failed = true;
            throw std::bad_alloc{};
        }
        --allocations_before_failure;
    }
    if (auto* allocation = std::malloc(size == 0 ? 1 : size)) {
        return allocation;
    }
    throw std::bad_alloc{};
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* allocation) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation) noexcept {
    std::free(allocation);
}
void operator delete(void* allocation, std::size_t) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation, std::size_t) noexcept {
    std::free(allocation);
}

// The runner may use nothrow temporary buffers. Route them through the same
// replacement so sanitizer allocation/deallocation families remain matched.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    return ::operator new(size, std::nothrow);
}
void operator delete(void* allocation, const std::nothrow_t&) noexcept {
    std::free(allocation);
}
void operator delete[](void* allocation, const std::nothrow_t&) noexcept {
    std::free(allocation);
}

namespace {

using corerpg::EntityId;
using corerpg::Persistence;
using corerpg::PersistenceStatus;

class EntityStructureAllocationTest : public ::testing::Test {
  protected:
    corerpg::WorldState source;
    corerpg::WorldState target;
    corerpg::RandomStreams source_random{123};
    corerpg::RandomStreams target_random{987};
    std::optional<corerpg::CatalogBuildResult> catalog;
    std::optional<corerpg::DefinitionId> definition;
    corerpg::CampaignSnapshot source_snapshot;
    corerpg::CampaignSnapshot target_snapshot;
    std::vector<std::uint8_t> bytes;

    void SetUp() override {
        catalog.emplace(corerpg_test::structural_catalog());
        definition.emplace(*corerpg::DefinitionId::parse("example.structural:unit"));
        const auto group = source.create_entity();
        static_cast<void>(source.create_entity());
        ASSERT_EQ(source.declare_population(group, 100), corerpg::PopulationChange::applied);
        ASSERT_EQ(source.materialize_population(group, 2).status,
                  corerpg::MaterializationStatus::applied);
        ASSERT_EQ(source.bind_definition(group, *catalog->catalog(), *definition),
                  corerpg::DefinitionBindingChange::applied);
        ASSERT_EQ(source.add_relation({EntityId{3}, corerpg::RelationKind{1}, group}),
                  corerpg::RelationChange::applied);
        ASSERT_EQ(source.place_entity(EntityId{3}, EntityId{2}),
                  corerpg::ContainmentChange::applied);
        static_cast<void>(source_random.stream(UINT64_MAX).next());
        const auto target_group = target.create_entity();
        ASSERT_EQ(target.declare_population(target_group, 12), corerpg::PopulationChange::applied);
        ASSERT_EQ(target.materialize_population(target_group, 1).status,
                  corerpg::MaterializationStatus::applied);
        ASSERT_EQ(target.bind_definition(EntityId{2}, *catalog->catalog(), *definition),
                  corerpg::DefinitionBindingChange::applied);
        static_cast<void>(target_random.stream(99).next());
        source_snapshot = Persistence::capture(source, source_random);
        target_snapshot = Persistence::capture(target, target_random);
        bytes = Persistence::serialize(source_snapshot).bytes;
    }

    template <typename Operation> void fail_each(Operation operation) {
        std::size_t failures = 0;
        bool completed = false;
        const auto* source_reference = source_random.find_stream(UINT64_MAX);
        const auto* target_reference = target_random.find_stream(99);
        const auto* definition_reference = source.definition_of(EntityId{1});
        for (std::size_t index = 0; index < 512; ++index) {
            bool threw = false;
            bool succeeded = false;
            {
                AllocationFailure failure{index};
                try {
                    succeeded = operation();
                } catch (const std::bad_alloc&) {
                    threw = true;
                }
            }
            ASSERT_EQ(threw, allocation_failed) << index;
            if (threw) {
                ++failures;
                EXPECT_EQ(Persistence::capture(source, source_random), source_snapshot) << index;
                EXPECT_EQ(Persistence::capture(target, target_random), target_snapshot) << index;
                EXPECT_EQ(Persistence::serialize(source_snapshot).bytes, bytes);
                EXPECT_EQ(source_random.find_stream(UINT64_MAX), source_reference);
                EXPECT_EQ(target_random.find_stream(99), target_reference);
                EXPECT_EQ(source.definition_of(EntityId{1}), definition_reference);
                continue;
            }
            EXPECT_TRUE(succeeded);
            completed = true;
            break;
        }
        EXPECT_TRUE(completed);
        EXPECT_GT(failures, 0U);
        RecordProperty("injected_allocation_failures", static_cast<int>(failures));
    }
};

TEST_F(EntityStructureAllocationTest, DefinitionBindingCopiesAndInsertionAreAtomic) {
    fail_each([&] {
        return source.bind_definition(EntityId{2}, *catalog->catalog(), *definition) ==
               corerpg::DefinitionBindingChange::applied;
    });
}

TEST_F(EntityStructureAllocationTest, RelationInsertionPreservesEveryOtherTable) {
    fail_each([&] {
        return source.add_relation({EntityId{2}, corerpg::RelationKind{77}, EntityId{1}}) ==
               corerpg::RelationChange::applied;
    });
}

TEST_F(EntityStructureAllocationTest, PopulationDeclarationPreservesEveryOtherTable) {
    fail_each([&] {
        return source.declare_population(EntityId{2}, UINT64_MAX) ==
               corerpg::PopulationChange::applied;
    });
}

TEST_F(EntityStructureAllocationTest, MaterializationFailureCannotEmitPartialIdentities) {
    fail_each([&] {
        return source.materialize_population(EntityId{1}, 5).status ==
               corerpg::MaterializationStatus::applied;
    });
}

TEST_F(EntityStructureAllocationTest, WholeWorldCopyAssignmentPreservesTheDestination) {
    fail_each([&] {
        target = source;
        return true;
    });
    EXPECT_EQ(Persistence::capture(target, source_random), source_snapshot);
}

TEST_F(EntityStructureAllocationTest, CapturePreservesBindingsRelationsPopulationAndRng) {
    fail_each([&] { return Persistence::capture(source, source_random) == source_snapshot; });
}

TEST_F(EntityStructureAllocationTest, SerializePreservesCompleteSnapshotAtEveryFailure) {
    fail_each([&] {
        return Persistence::serialize(source_snapshot).status == PersistenceStatus::success;
    });
}

TEST_F(EntityStructureAllocationTest, DeserializeNeverPublishesAPartialV2Candidate) {
    fail_each([&] { return Persistence::deserialize(bytes).status == PersistenceStatus::success; });
}

TEST_F(EntityStructureAllocationTest, RestorePreservesAllTablesAndRngAtEveryFailure) {
    fail_each([&] {
        return Persistence::restore(source_snapshot, target, target_random, {},
                                    catalog->catalog()) == PersistenceStatus::success;
    });
}

TEST_F(EntityStructureAllocationTest, LoadPreservesAllTablesAndRngAtEveryFailure) {
    fail_each([&] {
        return Persistence::load(bytes, target, target_random, {}, catalog->catalog()) ==
               PersistenceStatus::success;
    });
}

TEST_F(EntityStructureAllocationTest, QueriesAndNoOpsDoNotAllocateOrConsumeState) {
    bool unchanged = false;
    {
        AllocationFailure failure{0};
        unchanged =
            source.population_of(EntityId{1})->latent == 98 &&
            source.population_origin(EntityId{3}) == EntityId{1} &&
            source.definition_of(EntityId{1}) != nullptr &&
            source.relations_from(EntityId{3}).size() == 1 &&
            source.bind_definition(EntityId{1}, *catalog->catalog(), *definition) ==
                corerpg::DefinitionBindingChange::no_change &&
            source.add_relation({EntityId{3}, corerpg::RelationKind{1}, EntityId{1}}) ==
                corerpg::RelationChange::no_change &&
            source.remove_relation({EntityId{2}, corerpg::RelationKind{99}, EntityId{1}}) ==
                corerpg::RelationChange::no_change &&
            source.declare_population(EntityId{1}, 100) == corerpg::PopulationChange::no_change &&
            source.materialize_population(EntityId{1}, 0, 0).status ==
                corerpg::MaterializationStatus::no_change;
    }
    EXPECT_TRUE(unchanged);
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(Persistence::capture(source, source_random), source_snapshot);
}

} // namespace
