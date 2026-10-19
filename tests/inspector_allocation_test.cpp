#include <corerpg/persistence.hpp>
#include <corerpg/inspector.hpp>
#include "entity_structure_helpers.hpp"

#include <gtest/gtest.h>

#include <array>
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
using namespace corerpg;
TEST(InspectorAllocationTest, AllQueriesAndBudgetRejectionAllocateNothing) {
    WorldState world;
    const auto a = world.create_entity(), b = world.create_entity();
    const auto catalog = corerpg_test::structural_catalog();
    ASSERT_EQ(world.bind_definition(a, *catalog.catalog(), *DefinitionId::parse("example.structural:unit")),
              DefinitionBindingChange::applied);
    ASSERT_EQ(world.place_entity(a, b), ContainmentChange::applied);
    ASSERT_EQ(world.add_relation({a, RelationKind{7}, b}), RelationChange::applied);
    ASSERT_EQ(world.declare_population(b, 3), PopulationChange::applied);
    ASSERT_EQ(world.materialize_population(b, 1).status, MaterializationStatus::applied);
    ASSERT_EQ(world.learn_fact(b, {a, FactKind::existence, {}, {}}), KnowledgeChange::applied);
    RandomStreams random{42};
    static_cast<void>(random.stream(17).next());
    const auto before = Persistence::capture(world, random);
    const Inspector inspector{world};
    bool complete = false;
    InspectionDiagnostic diagnostic;
    {
        AllocationFailure failure{0};
        // No assertions while the injection is armed; any unexpected allocation throws.
        try {
            static_cast<void>(inspector.metadata());
            static_cast<void>(inspector.entity(a));
            static_cast<void>(inspector.entity(EntityId{99}));
            static_cast<void>(inspector.definition(a, *catalog.catalog()));
            static_cast<void>(inspector.entities());
            static_cast<void>(inspector.containment());
            static_cast<void>(inspector.relations());
            static_cast<void>(inspector.relations_from(a, {}, RelationKind{7}));
            static_cast<void>(inspector.populations());
            static_cast<void>(inspector.memberships());
            static_cast<void>(inspector.knowledge(b));
            static_cast<void>(inspector.knowledge(EntityId{99}));
            static_cast<void>(inspector.entities({0, 0}));
            static_cast<void>(inspector.relations({0, 1, 0}));
            diagnostic = inspector.diagnose({0, 0});
            complete = true;
        } catch (const std::bad_alloc&) {}
    }
    EXPECT_TRUE(complete);
    EXPECT_FALSE(allocation_failed);
    EXPECT_EQ(diagnostic.status, InspectionDiagnosticStatus::limit_exceeded);
    EXPECT_EQ(Persistence::capture(world, random), before);
}
TEST(InspectorAllocationTest, EveryLiveAndCandidateScratchFailureIsInconclusiveAndNonDestructive) {
    WorldState world;
    const auto a = world.create_entity(), b = world.create_entity();
    ASSERT_EQ(world.place_entity(a, b), ContainmentChange::applied);
    ASSERT_EQ(world.declare_population(b, 5), PopulationChange::applied);
    RandomStreams random{42};
    const auto before = Persistence::capture(world, random);
    const auto bytes_before = Persistence::serialize(before).bytes;
    const Inspector inspector{world};
    for (const bool candidate : {false, true}) {
        bool completed = false;
        std::size_t failures = 0;
        for (std::size_t index = 0; index < 64; ++index) {
            InspectionDiagnostic result;
            {
                AllocationFailure failure{index};
                result = candidate ? Inspector::diagnose_snapshot(before) : inspector.diagnose();
            }
            EXPECT_EQ(Persistence::capture(world, random), before);
            EXPECT_EQ(Persistence::serialize(before).bytes, bytes_before);
            if (allocation_failed) {
                ++failures;
                EXPECT_EQ(result.status, InspectionDiagnosticStatus::allocation_failure);
                EXPECT_EQ(result.severity, InspectionSeverity::warning);
                EXPECT_FALSE(result.valid());
                EXPECT_FALSE(result.code);
            } else {
                EXPECT_TRUE(result.valid());
                completed = true;
                break;
            }
        }
        EXPECT_TRUE(completed);
        EXPECT_GT(failures, 0U);
        RecordProperty(candidate ? "candidate_allocation_failures" : "live_allocation_failures",
                       static_cast<int>(failures));
    }
}
} // namespace
